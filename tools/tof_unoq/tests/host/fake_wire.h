// fake_wire.h - SYNTHETIC test double for the UNO Q Zephyr Wire + a VL53L8CX register file.
//
// Models only what the platform layer depends on, as documented in
// ArduinoCore-zephyr libraries/Wire/Wire.cpp (1.0.0):
//   * 256-byte TX and RX ring buffers: write() queues at most the free space,
//     requestFrom() returns 0 if more than 256 bytes are requested;
//   * endTransmission()/requestFrom() always end with STOP (stop flag ignored).
// and DS14161 Rev 7 section 4: a write sets the 16-bit index then stores bytes
// with auto-increment; a read returns bytes from the current index onward.
// It is NOT a model of the sensor's firmware, timing or ranging.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <vector>

struct FakeVl53 {
    uint8_t addr7 = 0x29;
    bool present = true;
    uint8_t device_id = 0xF0;
    uint8_t revision_id = 0x0C;
    std::vector<uint8_t> mem = std::vector<uint8_t>(65536, 0);
    uint16_t index = 0;

    uint8_t readByte() {
        uint8_t v = mem[index];
        if (mem[0x7FFF] == 0x00 && index == 0) v = device_id;
        if (mem[0x7FFF] == 0x00 && index == 1) v = revision_id;
        index = static_cast<uint16_t>(index + 1);
        return v;
    }
};

class FakeWire {
   public:
    explicit FakeWire(FakeVl53& dev) : dev_(dev) {}

    // Test controls / observations.
    int fail_next = 0;    // the next N bus operations NACK
    size_t short_by = 0;  // requestFrom() delivers this many bytes fewer than asked
    size_t max_tx_bytes = 0;
    size_t max_rx_bytes = 0;
    uint32_t tx_count = 0;
    uint32_t rx_count = 0;

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
        if (!dev_.present || addr_ != dev_.addr7) return 1;
        if (tx_len_ >= 2) {
            dev_.index = static_cast<uint16_t>((tx_[0] << 8) | tx_[1]);
            for (size_t i = 2; i < tx_len_; ++i) {
                dev_.mem[dev_.index] = tx_[i];
                dev_.index = static_cast<uint16_t>(dev_.index + 1);
            }
        }
        return 0;
    }

    size_t requestFrom(uint8_t addr, size_t n) {
        ++rx_count;
        rx_len_ = rx_pos_ = 0;
        if (n > sizeof(rx_)) return 0;  // Zephyr Wire: "wire buffer capacity ... is too small"
        if (n > max_rx_bytes) max_rx_bytes = n;
        if (fail_next > 0) {
            --fail_next;
            return 0;
        }
        if (!dev_.present || addr != dev_.addr7) return 0;
        const size_t give = n > short_by ? n - short_by : 0;
        for (size_t i = 0; i < give; ++i)
            rx_[i] = dev_.readByte();
        rx_len_ = give;
        return give;
    }

    int available() { return static_cast<int>(rx_len_ - rx_pos_); }
    int read() { return rx_pos_ < rx_len_ ? rx_[rx_pos_++] : -1; }

   private:
    FakeVl53& dev_;
    uint8_t addr_ = 0;
    uint8_t tx_[256];
    size_t tx_len_ = 0;
    uint8_t rx_[256];
    size_t rx_len_ = 0;
    size_t rx_pos_ = 0;
};
