// qwiic_peripherals.h - the trigger and the IMU, daisy-chained on the ToF's Qwiic bus.
//
//   UNO Q Qwiic (Wire1) -> SparkFun 6DoF BMI270 (0x68) -> SparkFun Qwiic Button
//   breakout BOB-15931 (0x6F, Omron D2F-01L on its button pads) -> Pololu
//   VL53L8CX (0x29). Wiring and gates: docs/hardware/tof_unoq/TRIGGER_IMU.md.
//
// Both devices use 8-bit register addresses, unlike the VL53L8CX's 16-bit
// index, so they get their own bounded register layer here (same retry policy
// and statistics as tof_i2c_platform.h). Every write ends with STOP and every
// read re-sends the register first: the Zephyr Wire ignores the no-stop flag
// (see tof_i2c_platform.h); both devices accept a STOP between the register
// write and the read (SparkFun's own Qwiic Button driver does exactly that).
//
// Register maps are taken from vendor sources, not datasheet memory:
//   Qwiic Button: sparkfun/SparkFun_Qwiic_Button_Arduino_Library src/registers.h
//                 and SparkFun_Qwiic_Button.h (DEV_ID 0x5D, default address 0x6F).
//   BMI270:       boschsensortec/BMI270_SensorAPI v2.113.0 bmi2_defs.h, and the
//                 init sequence of bmi2.c (bmi2_soft_reset -> write_config_file
//                 -> upload_file -> bmi2_get_internal_status).
//
// Header-only and Arduino-independent: tools/tof_unoq/tests/host drives this
// exact code against simulated register files. Simulated means the protocol
// is exercised, never that the parts work: that is the bench checklist's job.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "tof_i2c_platform.h"  // Stats, kCommsError

namespace qwiic {

template <typename Bus>
class RegBus8 {
   public:
    // TX ring is 256 bytes: one register byte + payload.
    static constexpr size_t kMaxWriteChunk = 128;
    static constexpr size_t kMaxReadChunk = 252;
    static constexpr int kRetries = 2;

    RegBus8(Bus& bus, uint8_t addr7, void (*delay_ms)(uint32_t))
        : bus_(bus), addr7_(addr7), delay_ms_(delay_ms) {}

    uint8_t address7() const { return addr7_; }
    const tofplat::Stats& stats() const { return stats_; }
    void resetStats() { stats_ = tofplat::Stats{}; }

    uint8_t write(uint8_t reg, const uint8_t* data, size_t n) {
        if (n > kMaxWriteChunk) return fail(reg, 1);  // callers chunk explicitly
        for (int attempt = 0; attempt <= kRetries; ++attempt) {
            if (attempt > 0) backoff();
            ++stats_.transactions;
            bus_.beginTransmission(addr7_);
            if (bus_.write(&reg, 1) != 1) continue;
            if (n > 0 && bus_.write(data, n) != n) continue;
            if (bus_.endTransmission() == 0) return 0;
        }
        return fail(reg, 1);
    }

    uint8_t write8(uint8_t reg, uint8_t v) { return write(reg, &v, 1); }

    uint8_t read(uint8_t reg, uint8_t* out, size_t n) {
        if (n > kMaxReadChunk) return fail(reg, 3);
        uint8_t stage = 0;
        for (int attempt = 0; attempt <= kRetries; ++attempt) {
            if (attempt > 0) backoff();
            stage = readOnce(reg, out, n);
            if (stage == 0) return 0;
        }
        return fail(reg, stage);
    }

    uint8_t read8(uint8_t reg, uint8_t& v) { return read(reg, &v, 1); }

   private:
    void backoff() {
        ++stats_.retries;
        if (delay_ms_) delay_ms_(1);
    }

    uint8_t readOnce(uint8_t reg, uint8_t* out, size_t n) {
        ++stats_.transactions;
        bus_.beginTransmission(addr7_);
        if (bus_.write(&reg, 1) != 1 || bus_.endTransmission() != 0) return 2;
        ++stats_.transactions;
        while (bus_.available() > 0)
            bus_.read();
        const size_t got = bus_.requestFrom(addr7_, n);
        if (got == 0) return 3;
        size_t i = 0;
        while (i < n && bus_.available() > 0)
            out[i++] = static_cast<uint8_t>(bus_.read());
        while (bus_.available() > 0)
            bus_.read();
        return (got == n && i == n) ? 0 : 4;
    }

    uint8_t fail(uint8_t reg, uint8_t stage) {
        ++stats_.failures;
        stats_.last_fail_reg = reg;
        stats_.last_fail_stage = stage;
        return tofplat::kCommsError;
    }

    Bus& bus_;
    uint8_t addr7_;
    void (*delay_ms_)(uint32_t);
    tofplat::Stats stats_{};
};

// ---- SparkFun Qwiic Button (BOB-15931) ---------------------------------------

namespace button {
constexpr uint8_t kAddr7 = 0x6F;
constexpr uint8_t kDevId = 0x5D;
constexpr uint8_t kRegId = 0x00;
constexpr uint8_t kRegFwMinor = 0x01;
constexpr uint8_t kRegFwMajor = 0x02;
constexpr uint8_t kRegStatus = 0x03;    // bit0 eventAvailable, bit1 hasBeenClicked, bit2 isPressed
constexpr uint8_t kRegDebounce = 0x05;  // uint16 little endian, ms
constexpr uint8_t kEventAvailable = 0x01;
constexpr uint8_t kHasBeenClicked = 0x02;
constexpr uint8_t kIsPressed = 0x04;
}  // namespace button

// One edge seen by poll(). `fromClickFlag` marks edges reconstructed from the
// latched hasBeenClicked flag: the press and release both happened between two
// polls, so their times are the poll time, not the switch time.
struct TriggerEdge {
    bool press = false;
    bool fromClickFlag = false;
};

template <typename Bus>
class QwiicButton {
   public:
    static constexpr uint16_t kDebounceMs =
        10;  // the button firmware's default, written explicitly

    QwiicButton(Bus& bus, void (*delay_ms)(uint32_t)) : regs_(bus, button::kAddr7, delay_ms) {}

    struct Identity {
        uint8_t id = 0, fwMajor = 0, fwMinor = 0;
        uint16_t debounceMs = 0;  // read back after writing kDebounceMs
        uint8_t comms = 0;        // 0 or kCommsError
    };

    // Reads the ID and firmware, sets the debounce, clears stale flags. ok() is
    // true only if the ID byte is the Qwiic Button's.
    Identity begin() {
        Identity r;
        uint8_t st = regs_.read8(button::kRegId, r.id);
        st |= regs_.read8(button::kRegFwMajor, r.fwMajor);
        st |= regs_.read8(button::kRegFwMinor, r.fwMinor);
        const uint8_t deb[2] = {static_cast<uint8_t>(kDebounceMs & 0xFF),
                                static_cast<uint8_t>(kDebounceMs >> 8)};
        st |= regs_.write(button::kRegDebounce, deb, 2);
        uint8_t back[2] = {0, 0};
        st |= regs_.read(button::kRegDebounce, back, 2);
        r.debounceMs = static_cast<uint16_t>(back[0] | (back[1] << 8));
        uint8_t status = 0;
        st |= regs_.read8(button::kRegStatus, status);
        st |= regs_.write8(button::kRegStatus, 0);  // drop clicks latched before we started
        pressed_ = (status & button::kIsPressed) != 0;
        r.comms = st ? tofplat::kCommsError : 0;
        ok_ = (r.comms == 0 && r.id == button::kDevId);
        failsInARow_ = 0;
        return r;
    }

    bool ok() const { return ok_; }
    bool pressed() const { return pressed_; }
    uint32_t presses() const { return presses_; }
    const tofplat::Stats& stats() const { return regs_.stats(); }

    // Polls the status register. Writes up to 2 edges into `out`, returns how
    // many, or -1 on a comms failure (after kOfflineAfter in a row, ok() goes
    // false and polling stops until begin() is called again).
    static constexpr uint8_t kOfflineAfter = 5;
    int poll(TriggerEdge out[2]) {
        if (!ok_) return -1;
        uint8_t status = 0;
        if (regs_.read8(button::kRegStatus, status) != 0) return commsFailure();
        const bool now = (status & button::kIsPressed) != 0;
        const bool clicked = (status & button::kHasBeenClicked) != 0;
        int n = 0;
        if (now && !pressed_) {
            out[n++] = {true, false};
        } else if (!now && pressed_) {
            out[n++] = {false, false};
        } else if (clicked && !now) {
            // A whole press+release between two polls: only the latch saw it.
            out[n++] = {true, true};
            out[n++] = {false, true};
        } else if (clicked && now) {
            // Released and pressed again between two polls.
            out[n++] = {false, true};
            out[n++] = {true, true};
        }
        for (int i = 0; i < n; ++i)
            if (out[i].press) ++presses_;
        pressed_ = now;
        if (status & (button::kEventAvailable | button::kHasBeenClicked))
            if (regs_.write8(button::kRegStatus, 0) != 0) return commsFailure();
        failsInARow_ = 0;
        return n;
    }

   private:
    int commsFailure() {
        if (++failsInARow_ >= kOfflineAfter) ok_ = false;
        return -1;
    }

    RegBus8<Bus> regs_;
    bool ok_ = false;
    bool pressed_ = false;
    uint32_t presses_ = 0;
    uint8_t failsInARow_ = 0;
};

// ---- Bosch BMI270 (SparkFun 6DoF BMI270 Qwiic) -------------------------------

namespace bmi270 {
constexpr uint8_t kAddr7 = 0x68;  // SparkFun board default (ADR jumper open); 0x69 if closed
constexpr uint8_t kChipId = 0x24;
constexpr uint8_t kRegChipId = 0x00;
constexpr uint8_t kRegErr = 0x02;
constexpr uint8_t kRegAccX = 0x0C;  // acc x,y,z then gyr x,y,z (int16 LE), then sensortime (24 bit)
constexpr uint8_t kRegInternalStatus = 0x21;
constexpr uint8_t kRegTemperature = 0x22;
constexpr uint8_t kRegAccConf = 0x40;
constexpr uint8_t kRegAccRange = 0x41;
constexpr uint8_t kRegGyrConf = 0x42;
constexpr uint8_t kRegGyrRange = 0x43;
constexpr uint8_t kRegInitCtrl = 0x59;
constexpr uint8_t kRegInitAddr0 = 0x5B;
constexpr uint8_t kRegInitData = 0x5E;
constexpr uint8_t kRegPwrConf = 0x7C;
constexpr uint8_t kRegPwrCtrl = 0x7D;
constexpr uint8_t kRegCmd = 0x7E;
constexpr uint8_t kCmdSoftReset = 0xB6;
constexpr uint8_t kInitOk = 0x01;  // INTERNAL_STATUS message field (bits 3:0)

// Configuration this driver applies (bmi2_defs.h field values):
//   ACC_CONF  0xA8 = filter_perf | bwp normal_avg4 (2<<4) | odr 100 Hz (0x08)
//   ACC_RANGE 0x01 = +-4 g   -> 8192 LSB/g
//   GYR_CONF  0xE8 = filter_perf | noise_perf | bwp normal (2<<4) | odr 100 Hz
//   GYR_RANGE 0x01 = +-1000 dps -> 32.768 LSB/dps
//   PWR_CTRL  0x0E = temp | acc | gyr enabled
//   PWR_CONF  0x00 = advanced power save off (no 450 us write spacing needed)
constexpr uint8_t kAccConf = 0xA8, kAccRange = 0x01, kGyrConf = 0xE8, kGyrRange = 0x01;
constexpr uint8_t kPwrCtrl = 0x0E;
constexpr int32_t kAccRangeG = 4, kGyrRangeDps = 1000;

inline int32_t accMilliG(int16_t raw) {
    return static_cast<int32_t>((static_cast<int64_t>(raw) * 1000 * kAccRangeG) / 32768);
}
inline int32_t gyrMilliDps(int16_t raw) {
    return static_cast<int32_t>((static_cast<int64_t>(raw) * 1000 * kGyrRangeDps) / 32768);
}
}  // namespace bmi270

struct ImuSample {
    int32_t acc_mg[3] = {0, 0, 0};
    int32_t gyr_mdps[3] = {0, 0, 0};
    uint32_t sensortime = 0;  // 24-bit, 39.0625 us per LSB (BMI270 SENSORTIME)
    bool tempValid = false;
    int32_t temp_cc = 0;  // centi-degC: 23 degC + raw / 512
};

template <typename Bus>
class Bmi270 {
   public:
    Bmi270(Bus& bus, void (*delay_ms)(uint32_t), uint8_t addr7 = bmi270::kAddr7)
        : regs_(bus, addr7, delay_ms), delay_ms_(delay_ms) {}

    enum class Stage : uint8_t { None, ChipId, Reset, Upload, InitStatus, Configure, Readback, Ok };
    static const char* stageName(Stage s) {
        switch (s) {
            case Stage::None:
                return "none";
            case Stage::ChipId:
                return "chip_id";
            case Stage::Reset:
                return "soft_reset";
            case Stage::Upload:
                return "config_upload";
            case Stage::InitStatus:
                return "init_status";
            case Stage::Configure:
                return "configure";
            case Stage::Readback:
                return "readback";
            case Stage::Ok:
                return "ok";
        }
        return "?";
    }

    struct Report {
        Stage stage = Stage::None;  // last stage reached; Ok on success
        uint8_t chipId = 0, internalStatus = 0, err = 0;
        uint8_t accConf = 0, accRange = 0, gyrConf = 0, gyrRange = 0, pwrCtrl = 0;
        uint32_t uploadMs = 0;
    };

    // Chip ID, soft reset, the Bosch 8 KB config upload, then the
    // configuration above, READ BACK. `config` is bmi270_config_file.
    // `millis` measures the upload (may be null).
    Report begin(const uint8_t* config, size_t configLen, uint32_t (*millis)() = nullptr) {
        Report r;
        ok_ = false;
        failsInARow_ = 0;
        r.stage = Stage::ChipId;
        if (regs_.read8(bmi270::kRegChipId, r.chipId) != 0 || r.chipId != bmi270::kChipId) return r;

        r.stage = Stage::Reset;
        if (regs_.write8(bmi270::kRegCmd, bmi270::kCmdSoftReset) != 0) return r;
        wait(2);  // bmi2_soft_reset: 2000 us
        // After reset advanced power save is on: one write, then 450 us before the next.
        if (regs_.write8(bmi270::kRegPwrConf, 0x00) != 0) return r;
        wait(1);

        r.stage = Stage::Upload;
        const uint32_t t0 = millis ? millis() : 0;
        if (regs_.write8(bmi270::kRegInitCtrl, 0x00) != 0) return r;
        static_assert(RegBus8<Bus>::kMaxWriteChunk % 2 == 0, "upload chunks are word addressed");
        for (size_t at = 0; at < configLen; at += RegBus8<Bus>::kMaxWriteChunk) {
            const size_t left = configLen - at;
            const size_t n =
                left < RegBus8<Bus>::kMaxWriteChunk ? left : RegBus8<Bus>::kMaxWriteChunk;
            // upload_file(): INIT_ADDR_0 = word bits 3:0, INIT_ADDR_1 = word bits 11:4.
            const uint16_t word = static_cast<uint16_t>(at / 2);
            const uint8_t addr[2] = {static_cast<uint8_t>(word & 0x0F),
                                     static_cast<uint8_t>(word >> 4)};
            if (regs_.write(bmi270::kRegInitAddr0, addr, 2) != 0) return r;
            if (regs_.write(bmi270::kRegInitData, config + at, n) != 0) return r;
        }
        if (regs_.write8(bmi270::kRegInitCtrl, 0x01) != 0) return r;
        r.uploadMs = millis ? millis() - t0 : 0;

        r.stage = Stage::InitStatus;
        // bmi2_get_internal_status() waits 20 ms; allow a little longer before failing.
        for (int i = 0; i < 15; ++i) {
            wait(i == 0 ? 20 : 10);
            if (regs_.read8(bmi270::kRegInternalStatus, r.internalStatus) != 0) return r;
            if ((r.internalStatus & 0x0F) == bmi270::kInitOk) break;
        }
        if ((r.internalStatus & 0x0F) != bmi270::kInitOk) return r;

        r.stage = Stage::Configure;
        const uint8_t acc[2] = {bmi270::kAccConf, bmi270::kAccRange};
        const uint8_t gyr[2] = {bmi270::kGyrConf, bmi270::kGyrRange};
        if (regs_.write8(bmi270::kRegPwrCtrl, bmi270::kPwrCtrl) != 0) return r;
        if (regs_.write(bmi270::kRegAccConf, acc, 2) != 0) return r;
        if (regs_.write(bmi270::kRegGyrConf, gyr, 2) != 0) return r;
        wait(50);  // gyro start-up is ~45 ms (datasheet: t_gyr_startup)

        r.stage = Stage::Readback;
        uint8_t conf[4] = {0, 0, 0, 0};
        if (regs_.read(bmi270::kRegAccConf, conf, 4) != 0) return r;
        if (regs_.read8(bmi270::kRegPwrCtrl, r.pwrCtrl) != 0) return r;
        if (regs_.read8(bmi270::kRegErr, r.err) != 0) return r;
        r.accConf = conf[0];
        r.accRange = conf[1];
        r.gyrConf = conf[2];
        r.gyrRange = conf[3];
        if (r.accConf != bmi270::kAccConf || r.accRange != bmi270::kAccRange ||
            r.gyrConf != bmi270::kGyrConf || r.gyrRange != bmi270::kGyrRange ||
            r.pwrCtrl != bmi270::kPwrCtrl || r.err != 0)
            return r;
        r.stage = Stage::Ok;
        ok_ = true;
        return r;
    }

    bool ok() const { return ok_; }
    const tofplat::Stats& stats() const { return regs_.stats(); }

    static constexpr uint8_t kOfflineAfter = 5;
    // Reads the latest sample. Returns false on a comms failure.
    bool read(ImuSample& s) {
        if (!ok_) return false;
        uint8_t d[15];
        if (regs_.read(bmi270::kRegAccX, d, sizeof(d)) != 0) return commsFailure();
        for (int i = 0; i < 3; ++i) {
            s.acc_mg[i] = bmi270::accMilliG(le16(d + 2 * i));
            s.gyr_mdps[i] = bmi270::gyrMilliDps(le16(d + 6 + 2 * i));
        }
        s.sensortime = static_cast<uint32_t>(d[12]) | (static_cast<uint32_t>(d[13]) << 8) |
                       (static_cast<uint32_t>(d[14]) << 16);
        uint8_t t[2];
        if (regs_.read(bmi270::kRegTemperature, t, 2) != 0) return commsFailure();
        const int16_t traw = le16(t);
        s.tempValid = traw != static_cast<int16_t>(0x8000);
        s.temp_cc = s.tempValid ? 2300 + (static_cast<int32_t>(traw) * 100) / 512 : 0;
        failsInARow_ = 0;
        return true;
    }

   private:
    static int16_t le16(const uint8_t* p) {
        return static_cast<int16_t>(static_cast<uint16_t>(p[0] | (p[1] << 8)));
    }
    void wait(uint32_t ms) {
        if (delay_ms_) delay_ms_(ms);
    }
    bool commsFailure() {
        if (++failsInARow_ >= kOfflineAfter) ok_ = false;
        return false;
    }

    RegBus8<Bus> regs_;
    void (*delay_ms_)(uint32_t);
    bool ok_ = false;
    uint8_t failsInARow_ = 0;
};

}  // namespace qwiic
