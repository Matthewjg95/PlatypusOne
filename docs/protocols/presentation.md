# Presentation Link Protocol v0.1

Wire protocol between PlatypusOS (Linux MPU, Qualcomm QRB2210) and an
**external display client** — a physically separate device that owns a panel,
its touch controller, and its buttons.

Status: **draft, framing implemented.** The shared byte-level codec is in place;
the session, tile encoder, transport, and display-client implementations remain.
The protocol was specified first so bring-up follows a reviewed contract rather
than a serial link invented at the bench. Per
[ADR-0001](../adr/0001-dynamic-linked-prototype-display.md), a linked external
display is the prototype display path while the production panel is undecided.

Companion spec: [mcu-bridge.md](mcu-bridge.md), which this deliberately
resembles — a developer who has read one can read the other.

## 1. Where the seam is

```
   apps/            launcher · shadowscan · viewer · measurement …
        │                  unaware the panel is remote
   services/renderer        draws into an RGB565 framebuffer
        │                   sized from hal::DisplayInfo
   platform/display  ┌──────────────────────────────┐
        │            │  hal::IDisplay               │  ← THE SEAM
        │            └──────────────────────────────┘
        │                     ▲                ▲
        │        Ili9341Display          LinkedDisplay
        │      (integrated panel,      (this protocol,
        │        future)                 prototype)
        ▼
   transport         USB CDC serial │ TCP over Wi-Fi │ …
        ▼
   display client    Tab5 / ESP32-S3 4.3" carrier — blits tiles, sends input
```

**The rule this protocol exists to enforce:** the external display is an
`IDisplay` implementation and nothing more. No app, no service, and no layout
code learns that the panel lives on another device. A later integrated panel
replaces `LinkedDisplay` with a local driver and nothing above the HAL changes.

Two consequences fall straight out:

- **The client is dumb by default.** It receives pixels and returns input
  events. It does not own the widget tree, the navigation state, or the app
  model. See §7 for the one deliberate exception.
- **Geometry is negotiated, never assumed.** The client reports its panel
  geometry in `HelloReply`; `LinkedDisplay::info()` returns what the client
  said. This is the runtime half of ADR-0001 — swapping a 320×240 prototype for
  an 800×480 one is a reconnect, not a rebuild.

## 2. Bandwidth — why tiles are mandatory

A full RGB565 frame is `width × height × 2` bytes:

| Panel | Full frame | Full frames/s at 1 MB/s (USB CDC) | at 4 MB/s (Wi-Fi TCP) |
|---|---:|---:|---:|
| 320×240 | 153 600 B | ~6.5 | ~26 |
| 800×480 | 768 000 B | ~1.3 | ~5.2 |

Sustaining 30 fps full-frame at 800×480 would need ~23 MB/s. No transport in
the prototype's reach delivers that. Therefore:

> **Full-frame push is a fallback, not the normal path.** The normal path is
> dirty-rectangle tiles.

Typical UI updates touch a small fraction of the screen — a launcher selection
highlight or a changing measurement readout is well under 10 % of the panel:

| Update | Dirty area | Bytes | fps at 1 MB/s |
|---|---:|---:|---:|
| Launcher row highlight (800×480) | ~4 % | ~31 kB | ~32 |
| Measurement readout redraw | ~8 % | ~61 kB | ~16 |
| Full-screen app switch | 100 % | 768 kB | ~1.3 |

Numbers above are **calculated, not measured**. Replace them with bench figures
during bring-up (see [open items](#11-open-items)).

This is what made `renderer/dirty-rects` (ROADMAP M2) a prerequisite rather
than an optimization. The renderer now supplies changed bounds; `LinkedDisplay`
will split those bounds into protocol-sized tiles. Optional
RLE (§6) buys another large factor on the flat-fill UI the renderer produces
today, but it does not remove the need for dirty rects.

## 3. Transport

Transport-agnostic: any reliable, ordered, byte-stream-like channel.

| Transport | Use | Notes |
|---|---|---|
| USB CDC serial | First bring-up proof | Lowest friction, per the Tab5 sprint plan. Default device `/dev/ttyACM0`. |
| TCP over Wi-Fi | Higher-bandwidth prototyping | Port **7423**. Both candidate clients have Wi-Fi; the UNO Q's radio is onboard. |
| Anything else | — | Must preserve ordering and deliver bytes intact; framing assumes no reordering. |

The framing below is applied identically on every transport, including TCP —
message boundaries are the protocol's own business, not the transport's.

## 4. Frame format

All multi-byte fields little-endian.

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | sync `0xB5` | resync point after corruption (distinct from the MCU bridge's `0xA5`) |
| 1 | 4 | `len` | payload byte count, max 65536 |
| 5 | 2 | `topic` | see §5 |
| 7 | 1 | `flags` | bit 0 = reply expected; bits 1–7 reserved, must be 0 |
| 8 | `len` | payload | topic-defined |
| 8+len | 2 | `crc16` | CRC-16/CCITT-FALSE (poly `0x1021`, init `0xFFFF`) over `len`+`topic`+`flags`+`payload` |

Header is 8 bytes; maximum frame is 65 546 bytes.

**Why this differs from the MCU bridge.** That protocol carries ≤1024-byte
control payloads, where a 16-bit length and CRC-8 are right-sized. Pixel tiles
are up to 64 KiB — 64× the MCU bridge's maximum — and CRC-8's error detection
degrades badly over payloads that large. The 32-bit length also leaves room to
raise the cap without a format change.

Frames failing CRC, or declaring `len > 65536`, are dropped silently; the
decoder resynchronizes on the next `0xB5`. Unknown topics **must be ignored,
never treated as errors** — this is what makes the protocol forward-parseable.

## 5. Topic allocation

| Range | Owner |
|---|---|
| `0x0000–0x000F` | Session and liveness |
| `0x0010–0x002F` | Display / framebuffer |
| `0x0030–0x004F` | Input (client → host) |
| `0x0050–0x00FF` | Reserved for core presentation services |
| `0x0100–0x01FF` | Optional semantic extensions (§7, non-normative) |
| `0x4000–0x7FFF` | User/experiment topics (`kUserBase`) |
| `0x8000–0xFFFF` | Reserved |

### Session

| Topic | Id | Direction | Payload |
|---|---|---|---|
| Hello | `0x0001` | host→client | `protoVersion u16, hostName[16] char` (NUL-padded) |
| HelloReply | `0x0002` | client→host | see below |
| Ping | `0x0003` | either | `nonce u32` |
| Pong | `0x0004` | either | `nonce u32` (echoed) |
| Bye | `0x0005` | either | empty — orderly shutdown; peer should not treat it as a fault |

`HelloReply` payload:

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | `protoVersion` | highest version the client supports |
| 2 | 2 | `width` | pixels — becomes `DisplayInfo::width` |
| 4 | 2 | `height` | pixels — becomes `DisplayInfo::height` |
| 6 | 1 | `pixelFormat` | `0` = RGB565 little-endian (only value defined in v0.1) |
| 7 | 1 | `capabilities` | bit 0 touch, bit 1 buttons, bit 2 encoder, bit 3 backlight control, bit 4 RLE16 decode |
| 8 | 4 | `maxPayload` | largest payload the client will accept, ≤ 65536 |
| 12 | 16 | `clientName` | NUL-padded, e.g. `"m5stack-tab5"` |

A session is usable only after a `Hello`/`HelloReply` exchange. Before it,
`LinkedDisplay::info()` has no honest answer to give and the board reports the
display as absent — the same `nullptr` degradation described in
[ARCHITECTURE.md](../ARCHITECTURE.md) §5, reached by a different route.

Version negotiation: the effective version is `min(host, client)`. A client
reporting a version the host cannot speak is a hard failure, logged and
surfaced as `Error::NotSupported`; the link is not used.

### Display

| Topic | Id | Direction | Payload |
|---|---|---|---|
| FrameBegin | `0x0010` | host→client | `frameId u16, tileCount u16` |
| Tile | `0x0011` | host→client | `frameId u16, x u16, y u16, w u16, h u16, encoding u8, reserved u8, data[]` |
| FrameEnd | `0x0012` | host→client | `frameId u16` — client may now blit/swap |
| FrameAck | `0x0013` | client→host | `frameId u16, status u8` (`0` = displayed, `1` = dropped) |
| Backlight | `0x0014` | host→client | `brightness u16` (0..0xFFFF = 0..100 %) |

Rules:

- Tiles must lie fully within the negotiated geometry. An out-of-bounds tile is
  a protocol error: the client drops the frame and replies `FrameAck{status=1}`.
- `data` length must equal the encoding's expected size for `w × h`
  (§6). Mismatch is a protocol error, handled as above.
- A `Tile` whose `frameId` does not match the open frame is discarded.
- Tiles within one frame may arrive in any order and must not overlap.

### Input

Client → host. Coordinates are in **panel space**, matching the negotiated
geometry; calibration and rotation are the client's or the driver's job, never
an app's.

| Topic | Id | Payload |
|---|---|---|
| Touch | `0x0030` | `type u8` (0 Down, 1 Move, 2 Up), `x u16, y u16` |
| Button | `0x0031` | `id u8, pressed u8` |
| Encoder | `0x0032` | `delta i16, pressed u8` |

`Touch` and `Button` map 1:1 onto `hal::TouchEvent` / `hal::ButtonEvent`.

`Encoder` has **no HAL representation yet.** The rotary encoder is the primary
navigation control in
[INDUSTRIAL_DESIGN.md](../hardware/INDUSTRIAL_DESIGN.md), but `hal::IDisplay`
exposes only touch and buttons. The topic is specified now so client firmware
can emit it from day one; until the HAL gains an encoder event, `LinkedDisplay`
translates a detent into button events and logs the loss of resolution. Closing
this properly is an ADR-0001 open item.

## 6. Tile encodings

| Id | Name | Data |
|---|---|---|
| `0` | Raw RGB565 | `w × h × 2` bytes, row-major, top-left origin, little-endian |
| `1` | RLE16 | run-length pairs: `count u16, pixel u16`; sum of counts must equal `w × h` |

`RLE16` may only be used when the client set capability bit 4. The host picks
per tile and must fall back to raw when RLE would expand the data — an
encoder that never checks can make flat-fill UI *larger*, which is the one
case RLE is supposed to win.

Tile height is bounded by `maxPayload`: a full-width raw tile on an 800-pixel
panel is 1600 bytes per row, so 64 KiB holds **40 rows**. A full 800×480 frame
is therefore at least 12 tiles. Encoders must split accordingly rather than
assuming one tile per update.

## 7. Optional semantic extensions (non-normative)

The [Tab5 prototype plan](../roadmap/TAB5_DISPLAY_PROTOTYPE.md) sketches
semantic messages — menu items, app state, measurement results, inference
results. Those are **not** how the UI is delivered, and topics `0x0100–0x01FF`
are reserved for them with two constraints:

1. **No app may depend on them.** Anything expressible only through semantic
   messages is a feature the integrated panel cannot render, which is the
   device-specific coupling this document exists to prevent.
2. **They are an offload, not a UI.** The intended use is the 3D viewer, where
   shipping a mesh once and letting the client's GPU orbit it beats pushing
   ~750 kB of pixels per frame over a 1 MB/s link. Even then, the authoritative
   UI chrome stays framebuffer-delivered.

Nothing in this range is defined in v0.1. The first candidate — `MeshPush` for
viewer offload — is deferred until the framebuffer path is measured, because
the measurement may show it is not needed.

## 8. Flow control and latency

- **One frame in flight.** The host does not send `FrameBegin` for frame *N+1*
  until it has received `FrameAck` for frame *N* or the ack has timed out
  (200 ms). This bounds memory on the client and keeps input latency from
  degrading behind a backlog of stale frames.
- **Dropping beats queueing.** On timeout the host abandons the frame and
  continues with the newest one. A stale frame is worth less than a responsive
  UI; `present()` returns `Error::Timeout` and the shell loop proceeds.
- **Input is never flow-controlled.** Input frames are small and must not wait
  behind pixel traffic. On a transport without prioritization the client should
  interleave input frames between tiles rather than after `FrameEnd`.
- **Liveness.** Either side may `Ping`; three consecutive unanswered pings at
  1 s intervals mark the link dead. `LinkedDisplay` then fails `present()` with
  `Error::IoFailure` and attempts reconnection in the background.

## 9. Mapping onto `hal::IDisplay`

| `IDisplay` member | Link behaviour |
|---|---|
| `info()` | Cached from `HelloReply`; valid only after a session is established |
| `setBacklight(f)` | `Backlight{brightness = f × 0xFFFF}`; `Error::NotSupported` if capability bit 3 is clear |
| `present(pixels)` | Full-frame fallback → bounded tiles → `FrameBegin`/`Tile`*/`FrameEnd`; blocks until `FrameAck` or timeout |
| `presentRegion(pixels, region)` | Split the renderer-provided dirty region into bounded tiles; same ack behavior |
| `onTouch(h)` | Handler invoked from the link's receive thread — **not** the shell thread |
| `onButton(h)` | Same threading caveat |

**Threading.** Input handlers run on the link receive thread, while
[ARCHITECTURE.md](../ARCHITECTURE.md) §7 requires `IApp` callbacks to run on the
shell thread. `LinkedDisplay` must therefore hand events across a queue rather
than calling app handlers directly — which makes `appfw/event-queue` (ROADMAP
M2) a hard dependency of this driver, not a nicety. Until that queue exists,
`LinkedDisplay` is not safe to wire into the composition root.

## 10. Versioning

v0.1 is a draft and may change incompatibly until the first working
host↔client build, at which point it becomes v1 and this line is removed.
After that: additive changes bump the minor version and must remain
forward-parseable (unknown topics ignored); any change to the frame header or
to an existing payload layout bumps the major version and is negotiated through
`Hello`/`HelloReply`.

Both sides must log the negotiated version at session start. Bring-up
transcripts that omit it are unreadable six weeks later.

## 11. Open items

- [ ] Reference implementation: `LinkedDisplay` under `platform/display/src/linked/`,
      plus a host-side loopback fake so the codec is testable with no hardware
      (the [CODING_STANDARDS](../CODING_STANDARDS.md) testing rule).
- [x] Shared framing codec header (`platypus/hal/link/Framing.hpp`), used
      verbatim by driver, client firmware, and unit tests — the pattern
      [mcu-bridge](mcu-bridge.md) already follows.
- [ ] Client firmware for the first target (Tab5), in its own repository or
      under a new top-level directory — decide placement before writing it.
- [ ] **Measure and replace the §2 estimates** with bench numbers for USB CDC
      and TCP, at both 320×240 and 800×480.
- [ ] Decide encoder representation in the HAL (ADR-0001 open item).
- [x] `appfw/event-queue` is ready before `LinkedDisplay` is wired into the
      composition root.
- [ ] Consider a `Hello` field for physical panel dimensions (mm), so DPI-aware
      layout and touch-target sizing become possible across a 3.2" and a 4.3"
      panel of similar pixel counts.
