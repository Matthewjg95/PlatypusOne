# MCU Bridge Protocol v1

Wire protocol between PlatypusOS (Linux MPU, Qualcomm QRB2210) and the
real-time firmware on the STM32U585, carried over an RPMsg/serial character
device (default `/dev/ttyRPMSG0`).

Reference implementation: [Framing.hpp](../../platform/hardware/include/platypus/hal/mcu/Framing.hpp)
(shared verbatim by the Linux driver, the MCU firmware, and host unit tests).

## Frame format

All multi-byte fields little-endian.

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 1 | sync `0xA5` | resync point after corruption |
| 1 | 2 | `len` | payload byte count, max 1024 |
| 3 | 2 | `topic` | see allocation table |
| 5 | len | payload | topic-defined |
| 5+len | 1 | `crc8` | poly 0x07, init 0x00, over len+topic+payload |

Frames failing CRC or exceeding max payload are dropped silently; the decoder
resynchronizes on the next `0xA5`. There is no acknowledgment at this layer —
request/reply topics provide it where needed.

## Topic allocation

| Range | Owner |
|---|---|
| `0x0000–0x0FFF` | Core I/O (below) |
| `0x1000–0x3FFF` | Reserved for future core services (sensors, power) |
| `0x4000–0x7FFF` | User/plugin topics (`kUserBase`) — `IMcuBridge::publish/subscribe` |
| `0x8000–0xFFFF` | Reserved |

### Core topics

| Topic | Id | Direction | Payload |
|---|---|---|---|
| Ping | `0x0001` | MPU→MCU | empty |
| Pong | `0x0002` | MCU→MPU | empty |
| PinMode | `0x0040` | MPU→MCU | `pin u8, mode u8` (PinMode enum) |
| GpioSet | `0x0010` | MPU→MCU | `pin u8, level u8` |
| GpioRead | `0x0011` | MPU→MCU | `pin u8` |
| GpioReadReply | `0x0012` | MCU→MPU | `pin u8, level u8` |
| AnalogRead | `0x0020` | MPU→MCU | `pin u8` |
| AnalogReadReply | `0x0021` | MCU→MPU | `pin u8, value u16` (raw ADC) |
| PwmWrite | `0x0030` | MPU→MCU | `pin u8, duty u16` (0..0xFFFF = 0..100%) |

Request/reply pairs use a bounded 100 ms wait on the MPU side
(`SerialMcuBridge::request`); timeout surfaces as `Error::Timeout`.

## Versioning

v1 has no version handshake; the Ping/Pong round-trip doubles as liveness and
compatibility probe. A `Hello` topic carrying protocol + firmware version is
planned before any breaking change (keep frames forward-parseable: unknown
topics must be ignored, never treated as errors).

## Open items

- [ ] MCU-side firmware module implementing this spec (Arduino sketch / Zephyr)
- [ ] Full baud-rate table in `SerialMcuBridge::open`
- [ ] Streaming sensor sample topics (`0x1000` range) for the IMU driver
