# tools/csi_bench

On-board evidence collector for the Arducam IMX219 bench-off (issue #40).
Procedure, configuration step and comparison table:
[docs/hardware/CSI_CAMERA_BENCH.md](../../docs/hardware/CSI_CAMERA_BENCH.md).

```bash
tools/csi_bench/csi_bench.sh B0394 all "csi port?, cable orientation, focus, distance, lighting, scene"
```

Runs as the normal user, needs `media-ctl`, `v4l2-ctl`, `cam` (libcamera) and
`python3` (all present on the UNO Q image). Output goes to
`evidence/csi/<SKU>/<UTC>-<test>/` (git-ignored) with a SHA256 manifest.
