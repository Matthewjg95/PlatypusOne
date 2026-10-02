# Commands — the short list

Everything we actually run on this project, grouped by where you type it.
Board address: `arduino@192.168.1.32` (DHCP; if it moves, check the router).
Anything with `sudo` on the board asks for the board's password.

## From the PC (PowerShell)

Run one board command without logging in (`-t` lets sudo ask for the password):

```powershell
ssh -t arduino@192.168.1.32 "sudo platypus-mode kiosk"
```

| Task | Command |
|---|---|
| Log in to the board | `ssh arduino@192.168.1.32` |
| Kiosk now **and on every boot** (desktop suppressed); power through the hub first | `ssh -t arduino@192.168.1.32 "sudo platypus-mode kiosk"` |
| Kiosk now, desktop again after a reboot | `ssh -t arduino@192.168.1.32 "sudo platypus-mode kiosk --once"` |
| Back to the desktop (now and on boot) | `ssh -t arduino@192.168.1.32 "sudo platypus-mode desktop"` |
| Which mode is running / boots | `ssh arduino@192.168.1.32 platypus-mode status` |
| Restart the kiosk (after a rebuild) | `ssh -t arduino@192.168.1.32 "sudo systemctl restart platypus-kiosk"` |
| Copy captures to the PC | `scp -r arduino@192.168.1.32:PlatypusOne/observations .` |
| Copy session bundles (DXF, JSON, PNG) | `scp -r arduino@192.168.1.32:PlatypusOne/sessions .` |
| ADB shell over USB-C (no hub) | `& "C:\Users\Public\adb\adb.exe" shell` |

`&&` does not work in Windows PowerShell 5.1. Inside the quoted ssh command it
is fine, because the board's shell runs it.

## On the board (after `ssh arduino@192.168.1.32`)

| Task | Command |
|---|---|
| Update the code | `cd ~/PlatypusOne && git pull` |
| Switch branch | `git fetch && git checkout <branch>` |
| Build the kiosk (~2 min) | `cmake --build build-bench --target scout_kiosk -j4` |
| Build and test everything | `cmake --build build-bench -j4 && ./build-bench/tests/platypus_tests` |
| Reinstall the kiosk service (after the unit file changes) | `sudo tools/kiosk/install.sh` |
| Kiosk log, live | `journalctl -u platypus-kiosk -f` |
| Newest captures | `ls -t ~/PlatypusOne/observations \| head` |
| List cameras (node numbers move between boots) | `v4l2-ctl --list-devices` |
| Camera controls | `v4l2-ctl -d /dev/video10 --list-ctrls` |
| Autofocus off / on | `v4l2-ctl -d /dev/video10 -c focus_automatic_continuous=0` (`=1` to restore) |
| Stop the desktop for a one-off test | `sudo systemctl stop lightdm` |

The Adesso webcam also has a manual focus ring on the lens: focus by hand while
watching the square's edges on the preview.

## On the PC: build, test, review

MSVC's CMake is not on PATH; in Git Bash:

```bash
export PATH="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin:$PATH"
```

| Task | Command |
|---|---|
| Build | `cmake --build build --config Debug` |
| Run all tests | `./build/tests/Debug/platypus_tests.exe` |
| Validation battery + report | `./build/tools/scout_validation/Debug/scout_validation.exe docs/contest/VALIDATION_REPORT.md` |
| Re-analyze a board capture with this build | `./build/tools/ui_preview/Debug/ui_preview.exe --analyze-yuyv scan-0048/source.yuyv` |
| Render the kiosk's session screens (no board) | `./build/tools/ui_preview/Debug/ui_preview.exe --kiosk-session-demo OUTDIR` |
| Format C++ (CI pins clang-format 18.1.8, installed with `pip install clang-format==18.1.8`) | `clang-format -i <files>` |

## STM32 side (LED matrix)

Board on USB-C directly to the PC (not through the hub), from `~/bin`:

```powershell
arduino-cli compile --fqbn arduino:zephyr:unoq firmware/led_matrix_platypus
arduino-cli upload -p COM7 --fqbn arduino:zephyr:unoq firmware/led_matrix_platypus
```

## When something is wrong

Harmless boot message: `gpucc-qcm2290 5990000.clock-controller: sync_state() pending due to 596a000.gmu`. The GPU (Adreno 702) has no real GMU; nothing binds to that node, so the clock controller logs this after the 30 s probe timeout. The GPU and display still work.


| Symptom | Fix |
|---|---|
| Black screen, kiosk never appears | `ssh -t arduino@192.168.1.32 "sudo platypus-mode desktop"`, then read `journalctl -u platypus-kiosk -b` |
| Kiosk says no camera | Webcam goes through the hub; the kiosk forces USB-C to host mode itself. Replug the hub, then restart the kiosk |
| Panel dark, no backlight | Cable orientation: see the photos in `docs/hardware/DSI_BRINGUP.md` |
| Screen empty after boot; `platypus-mode status` says NOTHING on screen | A mode switch was interrupted (board rebooted mid-command). Plug in through the hub, then `ssh -t arduino@192.168.1.32 "sudo platypus-mode kiosk"` |
| Desktop comes back after every reboot | Kiosk was started with `--once` or `systemctl start`; run `sudo platypus-mode kiosk` (no `--once`) |
| Captures blurry | Turn the lens focus ring until the square's edges are crisp on the preview |
| Screen warns "tilted" | Aim the camera straight down at the card; sizes read high when tilted |
