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

#include "../../../../firmware/tof_unoq_vl53l8cx/bmi270_config.h"
#include "../../../../firmware/tof_unoq_vl53l8cx/qwiic_peripherals.h"
#include "../../../../firmware/tof_unoq_vl53l8cx/tof_i2c_platform.h"
#include "../../../../firmware/tof_unoq_vl53l8cx/tof_proto.h"
#include "fake_qwiic.h"
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

// ---- trigger (Qwiic Button) and IMU (BMI270) ---------------------------------

static uint32_t g_clock_ms = 0;
static void tickDelay(uint32_t ms) {
    g_clock_ms += ms;
}
static uint32_t tickMillis() {
    return g_clock_ms;
}

using Button = qwiic::QwiicButton<FakeQwiicBus>;
using Imu = qwiic::Bmi270<FakeQwiicBus>;

static void test_button_begin_and_edges() {
    FakeQwiicButtonDev dev;
    FakeQwiicBus bus;
    bus.devices = {&dev};
    dev.press();
    dev.release();  // a click latched before the sketch started
    Button b(bus, tickDelay);
    const auto id = b.begin();
    CHECK(b.ok());
    CHECK(id.id == 0x5D && id.fwMajor == 1 && id.debounceMs == Button::kDebounceMs);
    CHECK((dev.regs[0x03] & 0x03) == 0);  // stale latches cleared
    qwiic::TriggerEdge e[2];
    CHECK(b.poll(e) == 0);  // ...so it is not reported as a press

    dev.press();
    CHECK(b.poll(e) == 1 && e[0].press && !e[0].fromClickFlag);
    CHECK(b.poll(e) == 0);  // held
    dev.release();
    CHECK(b.poll(e) == 1 && !e[0].press && !e[0].fromClickFlag);
    CHECK(b.presses() == 1);

    // A whole click between two polls: only the latch saw it.
    dev.press();
    dev.release();
    CHECK(b.poll(e) == 2 && e[0].press && !e[1].press && e[0].fromClickFlag);
    CHECK(b.presses() == 2);
    CHECK(b.poll(e) == 0);  // the latch was cleared, not reported twice

    // Held, then released and pressed again between two polls.
    dev.press();
    CHECK(b.poll(e) == 1);
    dev.release();
    dev.press();
    CHECK(b.poll(e) == 2 && !e[0].press && e[1].press);
    CHECK(b.presses() == 4 && b.pressed());
}

static void test_button_absent_and_lost() {
    FakeQwiicButtonDev dev;
    FakeQwiicBus bus;
    bus.devices = {&dev};
    dev.present = false;
    Button b(bus, tickDelay);
    b.begin();
    CHECK(!b.ok());
    qwiic::TriggerEdge e[2];
    CHECK(b.poll(e) == -1);

    dev.present = true;
    b.begin();
    CHECK(b.ok());
    dev.present = false;
    for (int i = 0; i < Button::kOfflineAfter; ++i)
        CHECK(b.poll(e) == -1);
    CHECK(!b.ok());  // bounded: goes offline instead of retrying forever
    const uint32_t tx = bus.tx_count;
    CHECK(b.poll(e) == -1 && bus.tx_count == tx);  // and stops touching the bus

    // A different device at 0x6F (wrong ID) is not accepted as the trigger.
    dev.present = true;
    dev.regs[0x00] = 0x5E;
    b.begin();
    CHECK(!b.ok());
}

static void test_bmi270_bring_up() {
    FakeBmi270Dev dev;
    FakeQwiicBus bus;
    bus.devices = {&dev};
    dev.expected = qwiic::bmi270::kConfigFile;
    dev.expectedLen = qwiic::bmi270::kConfigSize;
    dev.clockMs = &g_clock_ms;
    Imu imu(bus, tickDelay);
    const auto r = imu.begin(qwiic::bmi270::kConfigFile, qwiic::bmi270::kConfigSize, tickMillis);
    CHECK(imu.ok());
    CHECK(r.stage == Imu::Stage::Ok);
    CHECK(r.chipId == 0x24 && (r.internalStatus & 0x0F) == 1);
    CHECK(memcmp(dev.configRam.data(), qwiic::bmi270::kConfigFile, 8192) == 0);
    CHECK(dev.apsViolations == 0);
    CHECK(dev.initDataBursts == 8192 / 128 && dev.maxInitBurst == 128);
    CHECK(bus.max_tx_bytes <= 256);
    CHECK(r.accConf == 0xA8 && r.accRange == 0x01 && r.gyrConf == 0xE8 && r.gyrRange == 0x01);
    CHECK(r.pwrCtrl == 0x0E && r.err == 0);

    // At rest, flat: +1 g on z; a rotation on x; temperature 23 + 512/512.
    dev.setRaw(0x0C, 0);
    dev.setRaw(0x0E, -8192);
    dev.setRaw(0x10, 8192);
    dev.setRaw(0x12, 32767);
    dev.setRaw(0x14, -32768);
    dev.setRaw(0x16, 0);
    dev.regs[0x18] = 0x01;
    dev.regs[0x19] = 0x02;
    dev.regs[0x1A] = 0x03;
    dev.setRaw(0x22, 512);
    qwiic::ImuSample s;
    CHECK(imu.read(s));
    CHECK(s.acc_mg[0] == 0 && s.acc_mg[1] == -1000 && s.acc_mg[2] == 1000);
    CHECK(s.gyr_mdps[0] == 999969 && s.gyr_mdps[1] == -1000000 && s.gyr_mdps[2] == 0);
    CHECK(s.sensortime == 0x030201);
    CHECK(s.tempValid && s.temp_cc == 2400);
    dev.setRaw(0x22, static_cast<int16_t>(0x8000));
    CHECK(imu.read(s) && !s.tempValid);
}

static void test_bmi270_failures_are_reported_by_stage() {
    FakeBmi270Dev dev;
    FakeQwiicBus bus;
    bus.devices = {&dev};
    dev.clockMs = &g_clock_ms;
    Imu imu(bus, tickDelay);

    dev.present = false;
    CHECK(imu.begin(qwiic::bmi270::kConfigFile, 8192).stage == Imu::Stage::ChipId);
    dev.present = true;

    dev.regs[0x00] = 0x26;  // a BMI260/other part at 0x68 is not a BMI270
    const auto wrong = imu.begin(qwiic::bmi270::kConfigFile, 8192);
    CHECK(wrong.stage == Imu::Stage::ChipId && wrong.chipId == 0x26 && !imu.ok());
    dev.regs[0x00] = 0x24;

    // The part rejects the config (here: it expects different bytes).
    static uint8_t other[8192];
    memset(other, 0xA5, sizeof(other));
    dev.expected = other;
    dev.expectedLen = sizeof(other);
    const auto bad = imu.begin(qwiic::bmi270::kConfigFile, 8192);
    CHECK(bad.stage == Imu::Stage::InitStatus && (bad.internalStatus & 0x0F) == 2);
    CHECK(!imu.ok());

    // Comms lost while running: offline after a bounded number of failures.
    dev.expected = qwiic::bmi270::kConfigFile;
    CHECK(imu.begin(qwiic::bmi270::kConfigFile, 8192).stage == Imu::Stage::Ok);
    dev.present = false;
    qwiic::ImuSample s;
    for (int i = 0; i < Imu::kOfflineAfter; ++i)
        CHECK(!imu.read(s));
    CHECK(!imu.ok());
}

static void test_shared_bus_keeps_devices_apart() {
    FakeQwiicButtonDev btn;
    FakeBmi270Dev bmi;
    FakeQwiicBus bus;
    bus.devices = {&bmi, &btn};
    bmi.expected = qwiic::bmi270::kConfigFile;
    bmi.expectedLen = 8192;
    bmi.clockMs = &g_clock_ms;
    Button b(bus, tickDelay);
    Imu imu(bus, tickDelay);
    b.begin();
    imu.begin(qwiic::bmi270::kConfigFile, 8192);
    CHECK(b.ok() && imu.ok());
    btn.press();
    qwiic::TriggerEdge e[2];
    qwiic::ImuSample s;
    CHECK(imu.read(s));
    CHECK(b.poll(e) == 1 && e[0].press);
    // The button's ID register is untouched by the IMU's 8 KB upload.
    CHECK(btn.regs[0x00] == 0x5D && bmi.regs[0x00] == 0x24);
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
    L.begin("EVT");
    L.u64(700000);
    L.str("I");
    L.str("TRIGGER");
    L.kvs("edge", "press");
    L.kv("presses", 1);
    L.kvs("src", "level");
    if (L.finish()) fputs(buf, stdout);
    L.begin("IMU");
    L.u64(710000);
    L.u64(7);
    L.u64(0x030201);
    L.i32(12);
    L.i32(-8);
    L.i32(1001);
    L.i32(-150);
    L.i32(0);
    L.i32(999969);
    L.kvi("t_cc", -125);
    if (L.finish()) fputs(buf, stdout);
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
    test_button_begin_and_edges();
    test_button_absent_and_lost();
    test_bmi270_bring_up();
    test_bmi270_failures_are_reported_by_stage();
    test_shared_bus_keeps_devices_apart();
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
