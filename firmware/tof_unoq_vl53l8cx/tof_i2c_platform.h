// tof_i2c_platform.h - bounded VL53L8CX register I/O over an Arduino-Wire-like bus.
//
// Why not the stm32duino VL53L8CX C++ class's own I/O:
//  * its IO_Read() retries endTransmission() in an unbounded loop, so a missing
//    or unpowered sensor hangs the sketch instead of producing an error;
//  * the UNO Q Zephyr core's Wire ignores the "no stop" flag of
//    endTransmission(false)/requestFrom(..., false) (ArduinoCore-zephyr
//    libraries/Wire/Wire.cpp: ARG_UNUSED(stopBit)), so every index write is
//    followed by STOP. DS14161 Rev 7 section 4 allows a message to end with a
//    stop, and a read returns the register at the *current* index, so this is
//    valid for the VL53L8CX - but we re-send the index before every read chunk
//    rather than rely on the index surviving between separate read messages;
//  * the Zephyr Wire TX/RX ring buffers are 256 bytes (Wire.h), so a single
//    transaction carries at most 2 index bytes + 254 data bytes.
//
// Bus must provide (Arduino Wire API):
//   void beginTransmission(uint8_t addr7);
//   size_t write(const uint8_t* data, size_t n);   // returns bytes queued
//   uint8_t endTransmission();                      // 0 = ACKed and sent
//   size_t requestFrom(uint8_t addr7, size_t n);   // returns bytes received
//   int available();
//   int read();
//
// Header-only and Arduino-independent: tools/tof_unoq/tests drives this exact
// code against a simulated sensor on the host.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace tofplat {

// Matches UM3109 Rev 7 Table 5: 255 = major error, usually an I2C/SPI timeout.
constexpr uint8_t kCommsError = 255;

struct Stats {
    uint32_t transactions = 0;  // index writes + data writes + reads that were attempted
    uint32_t retries = 0;       // extra attempts after a failed transaction
    uint32_t failures = 0;      // operations that failed after all retries
    uint16_t last_fail_reg = 0;
    uint8_t last_fail_stage = 0;  // 1 = write, 2 = read index, 3 = read data, 4 = short read
};

template <typename Bus>
class RegisterBus {
   public:
    // Payload per write transaction: 256-byte ring minus the 2 index bytes.
    static constexpr size_t kMaxWriteChunk = 252;
    // Bytes per read transaction (<= the 256-byte RX ring).
    static constexpr size_t kMaxReadChunk = 252;
    // Extra attempts for a failed transaction before the operation fails.
    static constexpr int kRetries = 2;

    RegisterBus(Bus& bus, uint8_t addr7, void (*delay_ms)(uint32_t))
        : bus_(bus), addr7_(addr7), delay_ms_(delay_ms) {}

    uint8_t address7() const { return addr7_; }
    const Stats& stats() const { return stats_; }
    void resetStats() { stats_ = Stats{}; }

    // True if the device acknowledges a one-byte read. Non-destructive: the
    // VL53L8CX returns the byte at its current index. An ACK proves only that
    // something answers at this address - never that ranging works.
    bool probe() {
        while (bus_.available() > 0)
            bus_.read();
        const size_t got = bus_.requestFrom(addr7_, static_cast<size_t>(1));
        while (bus_.available() > 0)
            bus_.read();
        return got == 1;
    }

    uint8_t write(uint16_t reg, const uint8_t* data, uint32_t size) {
        uint32_t done = 0;
        do {
            const uint32_t left = size - done;
            const size_t n = left > kMaxWriteChunk ? kMaxWriteChunk : static_cast<size_t>(left);
            const uint16_t at = static_cast<uint16_t>(reg + done);
            bool ok = false;
            for (int attempt = 0; attempt <= kRetries && !ok; ++attempt) {
                if (attempt > 0) {
                    ++stats_.retries;
                    if (delay_ms_) delay_ms_(1);
                }
                ok = writeOnce(at, data + done, n);
            }
            if (!ok) return fail(at, 1);
            done += static_cast<uint32_t>(n);
        } while (done < size);
        return 0;
    }

    uint8_t read(uint16_t reg, uint8_t* out, uint32_t size) {
        uint32_t done = 0;
        while (done < size) {
            const uint32_t left = size - done;
            const size_t n = left > kMaxReadChunk ? kMaxReadChunk : static_cast<size_t>(left);
            const uint16_t at = static_cast<uint16_t>(reg + done);
            uint8_t stage = 0;
            bool ok = false;
            for (int attempt = 0; attempt <= kRetries && !ok; ++attempt) {
                if (attempt > 0) {
                    ++stats_.retries;
                    if (delay_ms_) delay_ms_(1);
                }
                stage = readOnce(at, out + done, n);
                ok = (stage == 0);
            }
            if (!ok) return fail(at, stage);
            done += static_cast<uint32_t>(n);
        }
        return 0;
    }

   private:
    bool writeOnce(uint16_t at, const uint8_t* data, size_t n) {
        ++stats_.transactions;
        const uint8_t index[2] = {static_cast<uint8_t>(at >> 8), static_cast<uint8_t>(at & 0xFF)};
        bus_.beginTransmission(addr7_);
        if (bus_.write(index, 2) != 2) return false;
        if (n > 0 && bus_.write(data, n) != n) return false;
        return bus_.endTransmission() == 0;
    }

    // 0 on success, else the failing stage (2 index, 3 data, 4 short read).
    uint8_t readOnce(uint16_t at, uint8_t* out, size_t n) {
        ++stats_.transactions;
        const uint8_t index[2] = {static_cast<uint8_t>(at >> 8), static_cast<uint8_t>(at & 0xFF)};
        bus_.beginTransmission(addr7_);
        if (bus_.write(index, 2) != 2 || bus_.endTransmission() != 0) return 2;
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

    uint8_t fail(uint16_t reg, uint8_t stage) {
        ++stats_.failures;
        stats_.last_fail_reg = reg;
        stats_.last_fail_stage = stage;
        return kCommsError;
    }

    Bus& bus_;
    uint8_t addr7_;
    void (*delay_ms_)(uint32_t);
    Stats stats_{};
};

}  // namespace tofplat
