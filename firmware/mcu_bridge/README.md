# mcu_bridge firmware (STM32U585)

The MCU side of [mcu-bridge v1](../../docs/protocols/mcu-bridge.md): Ping/Pong
plus the core GPIO / analog / PWM topics. Shares
`platform/hardware/include/platypus/hal/mcu/Framing.hpp` with the Linux driver
and the host tests — build with the include path, do not copy the header:

```bash
arduino-cli compile --fqbn <uno-q-mcu-fqbn> firmware/mcu_bridge \
  --build-property "compiler.cpp.extra_flags=-I$(pwd)/platform/hardware/include"
```

`<uno-q-mcu-fqbn>` is the UNO Q MCU board id from the Arduino UNO Q core
(check `arduino-cli board listall` after installing the UNO Q platform); a
bench Nucleo U575/U585 with the STM32duino core works for protocol testing
over USB CDC.

## Verification

Per STATUS.md protocol this sketch is **NOT COMPILED** until it builds against
the real UNO Q core — the RPMsg serial binding and `analogWrite` range are the
board-specific risks. Bring-up sequence is
[TEST_CHECKLISTS.md §2](../../docs/hardware/TEST_CHECKLISTS.md): Ping/Pong
round-trip, then GPIO loopback (jumper two pins, `GpioSet` one, `GpioRead`
the other).
