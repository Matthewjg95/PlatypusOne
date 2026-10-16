// tof_proto.h - line protocol between the UNO Q MCU sketch and the Linux logger.
//
// Every record is one ASCII line:
//
//     $TOF,<TYPE>,<field>,<field>,...*CCCC\n
//
// CCCC is CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no
// xor-out) of the bytes strictly between '$' and '*', as four upper-case hex
// digits. The Monitor path (MCU -> arduino-router -> TCP 127.0.0.1:7500) drops
// output while no client is connected and can split a record across writes, so
// the logger rejects any line whose CRC does not match and reports it.
//
// Header-only and free of Arduino dependencies so the host test
// (tools/tof_unoq/tests) compiles the SAME code the sketch uses.
//
// Record types (field order is part of the contract; see tools/tof_unoq/tofproto.py):
//   HELLO  mcu_us, k=v...                 sketch/protocol/driver identity
//   EVT    mcu_us, level(I|W|E), code, k=v...
//   CFG    mcu_us, k=v...                 sensor configuration actually applied
//   HB     mcu_us, k=v...                 heartbeat every few seconds
//   FRM    seq, streamcount, us_ready, us_read_done, zones, temp_c, z0, z1, ...
//          each zone = distance_mm:target_status:nb_target_detected:sigma_mm
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace tofproto {

constexpr int kProtocolVersion = 1;

inline uint16_t crc16_ccitt_false(const char* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(static_cast<uint8_t>(data[i])) << 8;
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

// Fixed-capacity line builder. Never allocates; on overflow it marks the line
// invalid and finish() returns 0, so a truncated record is never emitted.
class Line {
   public:
    Line(char* buf, size_t cap) : buf_(buf), cap_(cap) {
        if (cap_ > 0) buf_[0] = '\0';
    }

    void begin(const char* type) {
        len_ = 0;
        ok_ = true;
        raw("$TOF,");
        raw(type);
    }

    void str(const char* s) {
        raw(",");
        raw(s);
    }

    void u64(uint64_t v) {
        raw(",");
        rawU64(v);
    }

    void i32(int32_t v) {
        raw(",");
        if (v < 0) {
            raw("-");
            rawU64(static_cast<uint64_t>(-static_cast<int64_t>(v)));
        } else {
            rawU64(static_cast<uint64_t>(v));
        }
    }

    // ",key=value" with an unsigned value.
    void kv(const char* key, uint64_t v) {
        raw(",");
        raw(key);
        raw("=");
        rawU64(v);
    }

    // ",key=value" with a string value. Values must not contain ',', '*', '$'
    // or newlines; offending characters are replaced by '_'.
    void kvs(const char* key, const char* v) {
        raw(",");
        raw(key);
        raw("=");
        for (const char* p = v; *p; ++p) {
            const char c =
                (*p == ',' || *p == '*' || *p == '$' || *p == '\n' || *p == '\r') ? '_' : *p;
            put(c);
        }
    }

    // One zone "d:s:n:g".
    void zone(int32_t distance_mm, uint32_t status, uint32_t nb_targets, uint32_t sigma_mm) {
        raw(",");
        if (distance_mm < 0) {
            raw("-");
            rawU64(static_cast<uint64_t>(-static_cast<int64_t>(distance_mm)));
        } else {
            rawU64(static_cast<uint64_t>(distance_mm));
        }
        raw(":");
        rawU64(status);
        raw(":");
        rawU64(nb_targets);
        raw(":");
        rawU64(sigma_mm);
    }

    // Appends "*CCCC\n" and returns the total length, or 0 if the line overflowed.
    size_t finish() {
        if (!ok_) return 0;
        const uint16_t crc = crc16_ccitt_false(buf_ + 1, len_ - 1);  // skip '$'
        static const char hex[] = "0123456789ABCDEF";
        put('*');
        put(hex[(crc >> 12) & 0xF]);
        put(hex[(crc >> 8) & 0xF]);
        put(hex[(crc >> 4) & 0xF]);
        put(hex[crc & 0xF]);
        put('\n');
        if (!ok_ || len_ >= cap_) return 0;
        buf_[len_] = '\0';
        return len_;
    }

    const char* c_str() const { return buf_; }

   private:
    void put(char c) {
        if (len_ + 1 >= cap_) {  // keep room for the terminator
            ok_ = false;
            return;
        }
        buf_[len_++] = c;
    }

    void raw(const char* s) {
        while (*s)
            put(*s++);
    }

    void rawU64(uint64_t v) {
        char tmp[21];
        int n = 0;
        do {
            tmp[n++] = static_cast<char>('0' + (v % 10));
            v /= 10;
        } while (v != 0 && n < 20);
        while (n > 0)
            put(tmp[--n]);
    }

    char* buf_;
    size_t cap_;
    size_t len_ = 0;
    bool ok_ = true;
};

// Extends a wrapping 32-bit microsecond counter to 64 bits. Correct as long as
// it is called at least once per wrap period (~71.6 min); the sketch calls it
// every loop iteration.
class Clock64 {
   public:
    uint64_t extend(uint32_t now32) {
        if (now32 < last_) high_ += (uint64_t{1} << 32);
        last_ = now32;
        return high_ | now32;
    }

   private:
    uint64_t high_ = 0;
    uint32_t last_ = 0;
};

}  // namespace tofproto
