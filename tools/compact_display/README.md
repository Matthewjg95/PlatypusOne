# tools/compact_display

Software preparation for the compact DSI panels in
[COMPACT_DISPLAY_SELECTION.md](../../docs/hardware/COMPACT_DISPLAY_SELECTION.md):
the Waveshare 3.5inch DSI LCD (H) and (E) on UNO Q + UNO Media Carrier.

**None of this has run on a board yet.** Host tests prove consistency, not function.

| Path | Purpose |
|---|---|
| `panels/*.panel` | Panel definitions in the shell format of `dcuartielles/uno_q_dsi_displays` (pinned `d633636`), plus Goodix keys |
| `reference/waveshare_35dsi_overrides.json` | Values decoded from Waveshare's own `Waveshare_35DSI.dtbo`; the tests compare against it |
| `gen_overlay.py` | Overlay for a controller-less panel (polled GT911, no ATTINY) |
| `install_panel.sh` | Board-side install with backup and test-compose; `--dry-run` works anywhere |
| `restore_5in.sh` | Back to the proven 5" |
| `bench_accept.sh` | Acceptance stages D0–D7 → evidence + `results.json` |
| `test_compact_display.py` | Host unit tests |

```sh
python3 -m unittest tools/compact_display/test_compact_display.py
tools/compact_display/install_panel.sh --dry-run          # generate + compile the (H) overlay
```

Procedure: [COMPACT_DISPLAY_BENCH_ACCEPTANCE.md](../../docs/hardware/COMPACT_DISPLAY_BENCH_ACCEPTANCE.md).

The board scripts call upstream scripts for the module build. Upstream files
are not copied here: the upstream repository is the one that built our working
5", and we keep it as the single source.
