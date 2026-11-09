// fake_qwiic.h - SYNTHETIC test doubles: a multi-device Qwiic bus with a Qwiic
// Button and a BMI270 register file, behind the same Zephyr-Wire API as
// fake_wire.h (256-byte rings, every transaction ends with STOP).
//
// Modelled from the vendor sources the driver cites (qwiic_peripherals.h):
//   * Qwiic Button: status bit0 eventAvailable / bit1 hasBeenClicked are
//     latched and cleared by writing 0; bit2 isPressed follows the switch.
//   * BMI270: register pointer auto-increments, except INIT_DATA (0x5E), which
//     streams into the config RAM at the INIT_ADDR word address; INIT_CTRL = 1
//     sets INTERNAL_STATUS to init_ok only if the RAM holds the expected blob;
//     after a soft reset advanced power save is on and two writes closer than
//     450 us are a protocol violation (counted, as the part would misbehave).
// It is NOT a model of either part's timing, analogue behaviour or firmware.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

struct FakeDevice8 {
    virtual ~FakeDevice8() = default;
    uint8_t addr7 = 0;
    bool present = true;
    uint8_t ptr = 0;
    // A transaction's bytes after the register byte.
    virtual void writeRegs(uint8_t reg, const uint8_t* data, size_t n) = 0;
    virtual uint8_t readReg(uint8_t reg) = 0;
};

struct FakeQwiicButtonDev : FakeDevice8 {
    uint8_t regs[32] = {};
    bool switchClosed = false;
    FakeQwiicButtonDev() {
        addr7 = 0x6F;
        regs[0x00] = 0x5D;
        regs[0x01] = 0x03;  // firmware minor (arbitrary, synthetic)
        regs[0x02] = 0x01;  // firmware major
        regs[0x05] = 10;    // debounce default
    }
    void press() {
        switchClosed = true;
        regs[0x03] |= 0x04 | 0x01;
    }
    void release() {
        switchClosed = false;
        regs[0x03] = static_cast<uint8_t>((regs[0x03] & ~0x04) | 0x02 | 0x01);
    }
    void writeRegs(uint8_t reg, const uint8_t* d, size_t n) override {
        for (size_t i = 0; i < n; ++i) {
            const uint8_t r = static_cast<uint8_t>(reg + i);
            if (r == 0x00) continue;  // ID is read-only
            if (r == 0x03) {          // only the two latches are user-mutable
                regs[r] = static_cast<uint8_t>((d[i] & 0x03) | (switchClosed ? 0x04 : 0));
            } else if (r < sizeof(regs)) {
                regs[r] = d[i];
            }
        }
    }
    uint8_t readReg(uint8_t reg) override { return reg < sizeof(regs) ? regs[reg] : 0; }
};

struct FakeBmi270Dev : FakeDevice8 {
    uint8_t regs[256] = {};
    std::vector<uint8_t> configRam = std::vector<uint8_t>(8192, 0);
    const uint8_t* expected = nullptr;
    size_t expectedLen = 0;
    bool aps = true;  // advanced power save, on after reset
    uint32_t* clockMs = nullptr;
    uint32_t lastWriteMs = 0xFFFFFFFF;
    int apsViolations = 0;
    int initDataBursts = 0;
    size_t maxInitBurst = 0;

    FakeBmi270Dev() {
        addr7 = 0x68;
        reset();
    }
    void reset() {
        memset(regs, 0, sizeof(regs));
        regs[0x00] = 0x24;
        regs[0x7C] = 0x03;  // PWR_CONF default: adv_power_save on
        aps = true;
    }
    void setRaw(uint8_t reg, int16_t v) {
        regs[reg] = static_cast<uint8_t>(v & 0xFF);
        regs[reg + 1] = static_cast<uint8_t>((static_cast<uint16_t>(v) >> 8) & 0xFF);
    }
    void writeRegs(uint8_t reg, const uint8_t* d, size_t n) override {
        const uint32_t now = clockMs ? *clockMs : 0;
        if (aps && lastWriteMs == now) ++apsViolations;
        lastWriteMs = now;
        if (reg == 0x5E) {
            ++initDataBursts;
            if (n > maxInitBurst) maxInitBurst = n;
            size_t at = 2u * (static_cast<size_t>(regs[0x5B] & 0x0F) |
                              (static_cast<size_t>(regs[0x5C]) << 4));
            for (size_t i = 0; i < n && at < configRam.size(); ++i)
                configRam[at++] = d[i];
            return;
        }
        for (size_t i = 0; i < n; ++i) {
            const uint8_t r = static_cast<uint8_t>(reg + i);
            if (r == 0x7E && d[i] == 0xB6) {
                reset();
                return;
            }
            if (r == 0x00) continue;
            regs[r] = d[i];
            if (r == 0x7C) aps = (d[i] & 0x01) != 0;
            if (r == 0x59 && d[i] == 0x01) {
                const bool good = expected && expectedLen <= configRam.size() &&
                                  memcmp(configRam.data(), expected, expectedLen) == 0;
                regs[0x21] = good ? 0x01 : 0x02;  // init_ok / init_err
            }
        }
    }
    uint8_t readReg(uint8_t reg) override { return regs[reg]; }
};

class FakeQwiicBus {
   public:
    std::vector<FakeDevice8*> devices;
    int fail_next = 0;
    size_t max_tx_bytes = 0;
    uint32_t tx_count = 0;

    void beginTransmission(uint8_t addr) {
        addr_ = addr;
        tx_len_ = 0;
        rx_len_ = rx_pos_ = 0;
    }
    size_t write(const uint8_t* data, size_t n) {
        const size_t space = sizeof(tx_) - tx_len_;
        const size_t k = n < space ? n : space;
        memcpy(tx_ + tx_len_, data, k);
        tx_len_ += k;
        return k;
    }
    uint8_t endTransmission() {
        ++tx_count;
        if (tx_len_ > max_tx_bytes) max_tx_bytes = tx_len_;
        if (fail_next > 0) {
            --fail_next;
            return 1;
        }
        FakeDevice8* d = find(addr_);
        if (!d) return 1;
        if (tx_len_ >= 1) {
            d->ptr = tx_[0];
            if (tx_len_ > 1) d->writeRegs(tx_[0], tx_ + 1, tx_len_ - 1);
        }
        return 0;
    }
    size_t requestFrom(uint8_t addr, size_t n) {
        rx_len_ = rx_pos_ = 0;
        if (n > sizeof(rx_)) return 0;
        if (fail_next > 0) {
            --fail_next;
            return 0;
        }
        FakeDevice8* d = find(addr);
        if (!d) return 0;
        for (size_t i = 0; i < n; ++i)
            rx_[i] = d->readReg(static_cast<uint8_t>(d->ptr + i));
        rx_len_ = n;
        return n;
    }
    int available() { return static_cast<int>(rx_len_ - rx_pos_); }
    int read() { return rx_pos_ < rx_len_ ? rx_[rx_pos_++] : -1; }

   private:
    FakeDevice8* find(uint8_t addr) {
        for (FakeDevice8* d : devices)
            if (d->present && d->addr7 == addr) return d;
        return nullptr;
    }
    uint8_t addr_ = 0;
    uint8_t tx_[256];
    size_t tx_len_ = 0;
    uint8_t rx_[256];
    size_t rx_len_ = 0;
    size_t rx_pos_ = 0;
};
