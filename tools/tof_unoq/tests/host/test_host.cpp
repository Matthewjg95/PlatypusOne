// Host tests for the firmware's Arduino-independent headers (SYNTHETIC data only).
//
//   test_host            run the platform-layer and protocol checks; exit 0 on success
//   test_host emit       print sample protocol lines for the Python parser test
//
// Built by tools/tof_unoq/tests/test_firmware_host.py. When TOF_WITH_ULD is
// defined, ST's real vl53l8cx_is_alive() runs through the platform layer
// against the fake register file.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../../firmware/tof_unoq_vl53l8cx/tof_i2c_platform.h"
#include "../../../../firmware/tof_unoq_vl53l8cx/tof_proto.h"
#include "fake_wire.h"

#ifdef TOF_WITH_ULD
extern "C" {
#include "vl53l8cx_api.h"
}
#endif

static int g_failures = 0;
#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

static void noDelay(uint32_t) {}

using Bus = tofplat::RegisterBus<FakeWire>;

static void test_probe() {
    FakeVl53 dev;
    FakeWire w(dev);
    Bus bus(w, 0x29, noDelay);
    CHECK(bus.probe());
    dev.present = false;
    CHECK(!bus.probe());
}

static void test_write_chunking_firmware_sized() {
    FakeVl53 dev;
    FakeWire w(dev);
    Bus bus(w, 0x29, noDelay);
    // ULD uploads firmware in 0x8000-byte WrMulti calls starting at index 0.
    static uint8_t blob[0x8000];
    for (size_t i = 0; i < sizeof(blob); ++i)
        blob[i] = static_cast<uint8_t>(i * 7 + 3);
    CHECK(bus.write(0x0000, blob, sizeof(blob)) == 0);
    CHECK(memcmp(dev.mem.data(), blob, sizeof(blob)) == 0);
    CHECK(w.max_tx_bytes <= 256);
    CHECK(w.max_tx_bytes == 2 + Bus::kMaxWriteChunk);
    CHECK(w.tx_count == (sizeof(blob) + Bus::kMaxWriteChunk - 1) / Bus::kMaxWriteChunk);
    CHECK(bus.stats().failures == 0);
}

static void test_read_chunking_results_sized() {
    FakeVl53 dev;
    FakeWire w(dev);
    Bus bus(w, 0x29, noDelay);
    dev.mem[0x7FFF] = 0x02;  // not page 0: plain memory
    for (int i = 0; i < 1500; ++i)
        dev.mem[0x0100 + i] = static_cast<uint8_t>(i ^ 0x5A);
    uint8_t out[1500];
    memset(out, 0, sizeof(out));
    CHECK(bus.read(0x0100, out, sizeof(out)) == 0);
    bool same = true;
    for (int i = 0; i < 1500; ++i)
        same = same && out[i] == static_cast<uint8_t>(i ^ 0x5A);
    CHECK(same);
    CHECK(w.max_rx_bytes <= Bus::kMaxReadChunk);
}

static void test_transient_failure_is_retried() {
    FakeVl53 dev;
    FakeWire w(dev);
    Bus bus(w, 0x29, noDelay);
    uint8_t v = 0x42;
    w.fail_next = 1;
    CHECK(bus.write(0x1234, &v, 1) == 0);
    CHECK(dev.mem[0x1234] == 0x42);
    CHECK(bus.stats().retries == 1);
    CHECK(bus.stats().failures == 0);
}

static void test_persistent_failure_is_bounded_and_reported() {
    FakeVl53 dev;
    FakeWire w(dev);
    Bus bus(w, 0x29, noDelay);
    dev.present = false;
    uint8_t out[4] = {0};
    CHECK(bus.read(0x0000, out, 4) == tofplat::kCommsError);
    CHECK(bus.stats().failures == 1);
    CHECK(bus.stats().last_fail_stage == 2);  // index write NACKed
    CHECK(w.tx_count == 1 + Bus::kRetries);   // bounded: no infinite retry loop
    uint8_t v = 1;
    CHECK(bus.write(0x0010, &v, 1) == tofplat::kCommsError);
    CHECK(bus.stats().last_fail_stage == 1);
}

static void test_short_read_is_an_error() {
    FakeVl53 dev;
    FakeWire w(dev);
    Bus bus(w, 0x29, noDelay);
    uint8_t out[8];
    w.short_by = 1;  // device answers but delivers 7 of 8 bytes, every time
    CHECK(bus.read(0x0040, out, sizeof(out)) == tofplat::kCommsError);
    CHECK(bus.stats().last_fail_stage == 4);
    CHECK(w.rx_count == 1 + Bus::kRetries);
    w.short_by = 0;
    // The platform never asks Zephyr Wire for more than its 256-byte RX ring.
    uint8_t big[1024];
    CHECK(bus.read(0x2000, big, sizeof(big)) == 0);
    CHECK(w.max_rx_bytes <= 256);
}

static void test_protocol_crc_and_line() {
    // CRC-16/CCITT-FALSE check value for "123456789" is 0x29B1.
    CHECK(tofproto::crc16_ccitt_false("123456789", 9) == 0x29B1);
    char buf[64];
    tofproto::Line L(buf, sizeof(buf));
    L.begin("HB");
    L.u64(42);
    L.kvs("state", "IDLE,bad*chars");
    const size_t n = L.finish();
    CHECK(n > 0);
    CHECK(strstr(buf, "IDLE_bad_chars") != nullptr);
    CHECK(buf[n - 1] == '\n');
    // Overflow never emits a truncated line.
    char tiny[16];
    tofproto::Line T(tiny, sizeof(tiny));
    T.begin("FRM");
    for (int i = 0; i < 10; ++i)
        T.zone(1234, 5, 1, 12);
    CHECK(T.finish() == 0);
}

static void test_clock64_wrap() {
    tofproto::Clock64 c;
    CHECK(c.extend(0xFFFFFF00u) == 0xFFFFFF00ull);
    CHECK(c.extend(0x00000010u) == 0x100000010ull);
    CHECK(c.extend(0x00000020u) == 0x100000020ull);
}

#ifdef TOF_WITH_ULD
static FakeVl53* g_dev;
static FakeWire* g_wire;
static Bus* g_bus;
static uint8_t pw(void* h, uint16_t r, uint8_t* p, uint32_t n) {
    return static_cast<Bus*>(h)->write(r, p, n);
}
static uint8_t pr(void* h, uint16_t r, uint8_t* p, uint32_t n) {
    return static_cast<Bus*>(h)->read(r, p, n);
}
static uint8_t pwait(void*, uint32_t) {
    return 0;
}

static void test_uld_is_alive_through_platform() {
    static VL53L8CX_Configuration cfg;
    FakeVl53 dev;
    FakeWire w(dev);
    Bus bus(w, 0x29, noDelay);
    g_dev = &dev;
    g_wire = &w;
    g_bus = &bus;
    memset(&cfg, 0, sizeof(cfg));
    cfg.platform.address = 0x52;
    cfg.platform.Write = pw;
    cfg.platform.Read = pr;
    cfg.platform.Wait = pwait;
    cfg.platform.handle = &bus;
    uint8_t alive = 9;
    CHECK(vl53l8cx_is_alive(&cfg, &alive) == 0);
    CHECK(alive == 1);
    CHECK(dev.mem[0x7FFF] == 0x02);  // ULD leaves the device on page 2
    dev.device_id = 0xAA;
    CHECK(vl53l8cx_is_alive(&cfg, &alive) == 0);
    CHECK(alive == 0);
    dev.present = false;
    CHECK(vl53l8cx_is_alive(&cfg, &alive) != 0);  // comms error surfaces, no hang
}
#endif

static int emit() {
    char buf[1600];
    tofproto::Line L(buf, sizeof(buf));
    L.begin("HELLO");
    L.u64(1000);
    L.kv("proto", tofproto::kProtocolVersion);
    L.kvs("sketch", "tof_unoq_vl53l8cx host-test");
    if (L.finish()) fputs(buf, stdout);
    L.begin("EVT");
    L.u64(2000);
    L.str("I");
    L.str("ACK");
    L.kv("addr7", 0x29);
    if (L.finish()) fputs(buf, stdout);
    for (int seq = 0; seq < 3; ++seq) {
        L.begin("FRM");
        L.u64(seq);
        L.u64(static_cast<uint64_t>(seq + 1));
        L.u64(100000ull + seq * 200000ull);
        L.u64(108000ull + seq * 200000ull);
        L.u64(64);
        L.i32(-3 + seq);
        for (int z = 0; z < 64; ++z) {
            const int status = (z == 0) ? 255 : (z == 63 ? 6 : 5);
            L.zone(z == 1 ? -5 : 500 + z, status, z == 0 ? 0 : 1, 7 + (z % 5));
        }
        if (L.finish()) fputs(buf, stdout);
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "emit") == 0) return emit();
    test_probe();
    test_write_chunking_firmware_sized();
    test_read_chunking_results_sized();
    test_transient_failure_is_retried();
    test_persistent_failure_is_bounded_and_reported();
    test_short_read_is_an_error();
    test_protocol_crc_and_line();
    test_clock64_wrap();
#ifdef TOF_WITH_ULD
    test_uld_is_alive_through_platform();
    puts("ULD is_alive integration: ran");
#else
    puts("ULD is_alive integration: skipped (no ULD sources)");
#endif
    if (g_failures) {
        fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    puts("all host checks passed");
    return 0;
}
