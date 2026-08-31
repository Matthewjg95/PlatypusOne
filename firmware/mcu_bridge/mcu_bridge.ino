// PlatypusOS MCU firmware — mcu-bridge v1 (STM32U585 side).
//
// Implements docs/protocols/mcu-bridge.md: Ping/Pong liveness plus the core
// GPIO/analog/PWM topics — the smallest proof of the dual-brain architecture
// (TEST_CHECKLISTS.md section 2 is written against exactly this).
//
// The wire framing is the SAME header the Linux driver and host tests use:
// build with the repository include path, never a copied file —
//   arduino-cli compile --fqbn <uno-q-mcu-fqbn> firmware/mcu_bridge \
//     --build-property "compiler.cpp.extra_flags=-I<repo>/platform/hardware/include"
//
// Transport: an Arduino Stream. On the UNO Q the sketch talks to Linux over
// the RPMsg-backed serial the core exposes; on a bench Nucleo it is the USB
// CDC monitor — the protocol does not care. Unknown topics are ignored by
// contract; malformed core payloads are dropped.
#include <platypus/hal/mcu/Framing.hpp>

namespace mcu = platypus::hal::mcu;

namespace {

mcu::Decoder decoder;

void sendFrame(std::uint16_t topic, const std::uint8_t* payload, std::size_t count) {
    const auto frame =
        mcu::encode(topic, {reinterpret_cast<const std::byte*>(payload), count});
    Serial.write(reinterpret_cast<const std::uint8_t*>(frame.data()), frame.size());
}

// hal::PinMode wire values (IMcuBridge.hpp): Input=0, InputPullup=1,
// Output=2, Analog=3, Pwm=4.
void applyPinMode(std::uint8_t pin, std::uint8_t mode) {
    switch (mode) {
        case 0: pinMode(pin, INPUT); break;
        case 1: pinMode(pin, INPUT_PULLUP); break;
        case 2: pinMode(pin, OUTPUT); break;
        case 3:
#ifdef INPUT_ANALOG
            pinMode(pin, INPUT_ANALOG);
#else
            pinMode(pin, INPUT);
#endif
            break;
        case 4: pinMode(pin, OUTPUT); break;
        default: break;  // unknown mode: ignore by contract
    }
}

void handle(const mcu::Message& msg) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(msg.payload.data());
    switch (msg.topic) {
        case mcu::topics::kPing:
            sendFrame(mcu::topics::kPong, nullptr, 0);
            break;
        case mcu::topics::kPinMode:
            if (msg.payload.size() == 2) applyPinMode(p[0], p[1]);
            break;
        case mcu::topics::kGpioSet:
            if (msg.payload.size() == 2) digitalWrite(p[0], p[1] ? HIGH : LOW);
            break;
        case mcu::topics::kGpioRead:
            if (msg.payload.size() == 1) {
                const std::uint8_t reply[2] = {
                    p[0], static_cast<std::uint8_t>(digitalRead(p[0]) == HIGH ? 1 : 0)};
                sendFrame(mcu::topics::kGpioReadReply, reply, sizeof(reply));
            }
            break;
        case mcu::topics::kAnalogRead:
            if (msg.payload.size() == 1) {
                const auto raw = static_cast<std::uint16_t>(analogRead(p[0]));
                const std::uint8_t reply[3] = {p[0], static_cast<std::uint8_t>(raw & 0xFF),
                                               static_cast<std::uint8_t>(raw >> 8)};
                sendFrame(mcu::topics::kAnalogReadReply, reply, sizeof(reply));
            }
            break;
        case mcu::topics::kPwmWrite:
            if (msg.payload.size() == 3) {
                const auto duty =
                    static_cast<std::uint16_t>(p[1] | (static_cast<std::uint16_t>(p[2]) << 8));
                // 0..0xFFFF -> the core's 8-bit analogWrite range.
                analogWrite(p[0], duty >> 8);
            }
            break;
        default:
            break;  // unknown topic: ignored, never an error (forward compat)
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
}

void loop() {
    while (Serial.available() > 0) {
        if (auto msg = decoder.feed(std::byte{static_cast<std::uint8_t>(Serial.read())}))
            handle(*msg);
    }
}
