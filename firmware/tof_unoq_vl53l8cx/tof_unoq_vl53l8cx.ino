// tof_unoq_vl53l8cx - Pololu #3419 (VL53L8CX) on the Arduino UNO Q Qwiic connector.
//
// Runs on the STM32U585 MCU (Arduino UNO Q, core arduino:zephyr, FQBN
// arduino:zephyr:unoq). Qwiic = STM32 I2C4 on PD13 (SDA) / PD12 (SCL), which the
// core exposes as `Wire1` (variant arduino_uno_q_stm32u585xx: i2cs = <&i2c2>,
// <&i2c4>, <&i2c3> -> Wire, Wire1, Wire2). The Linux side cannot reach this bus.
//
// Output goes through the Arduino_RouterBridge `Monitor`, which arduino-router
// on Linux republishes as a raw TCP stream on 127.0.0.1:7500. Run
// tools/tof_unoq/tof_log.py on the UNO Q FIRST: this sketch waits until a
// monitor client is connected so the whole bring-up sequence is recorded.
//
// Bring-up sequence, each step reported as an EVT record (protocol: tof_proto.h):
//   BUS_BEGIN -> SCAN (address list) -> ACK 0x29 -> ID (page 0 regs 0/1 must be
//   0xF0/0x0C, the ULD vl53l8cx_is_alive() check) -> INIT (~84 KB firmware
//   upload by ST ULD vl53l8cx_init) -> CFG (values READ BACK from the sensor)
//   -> RANGING_START -> FRM records.
// An ACK alone is never reported as success; only FRM records prove ranging.
//
// Commands (one per line, sent by the logger to the monitor socket):
//   STATUS            re-emit HELLO, CFG and a heartbeat
//   STOP / START      stop ranging / start (or re-run bring-up after an error)
//   RES 4|8           resolution 4x4 or 8x8 (only while stopped)
//   HZ <n>            ranging frequency (only while stopped; ULD enforces limits)
//   SCAN              rescan the bus (only while stopped)
//
// Dependencies (pinned in sketch.yaml): Arduino_RouterBridge 0.4.3,
// STM32duino VL53L8CX 2.1.0 (ST ULD API VL53L8CX_1.3.0). Only the ULD C API is
// used; the library's C++ class and its unbounded-retry I/O are not.

#include <Arduino_RouterBridge.h>
#include <Wire.h>

#include "tof_i2c_platform.h"
#include "tof_proto.h"

extern "C" {
#include <vl53l8cx_api.h>
}

#define SKETCH_VERSION "0.1.0"

namespace {

// ---- configuration (defaults; RES/HZ commands can change the last two) -------
constexpr uint8_t kAddr7 = 0x29;     // VL53L8CX default 0x52 >> 1 (DS14161 section 4)
constexpr uint32_t kI2cHz = 400000;  // core default for I2C4; Pololu shifter is load-sensitive
constexpr uint32_t kHeartbeatMs = 5000;
constexpr uint32_t kCommandPollMs = 250;  // each Monitor.available() is an RPC round trip
constexpr uint32_t kReadyPollMs = 2;
constexpr uint8_t kStallLimit = 3;  // consecutive data-ready timeouts before giving up
uint8_t g_resolution = VL53L8CX_RESOLUTION_8X8;
uint8_t g_hz = 5;  // 8x8 frames are ~0.8 KB of text; Bridge UART is 115200 baud

TwoWire& tofWire = Wire1;

void delayMs(uint32_t ms) {
    delay(ms);
}
void configure_report_only();

tofplat::RegisterBus<TwoWire> g_bus(tofWire, kAddr7, delayMs);
VL53L8CX_Configuration g_dev;  // ~4 KB with ULD buffers: keep off the stack
VL53L8CX_ResultsData g_results;

char g_buf[1536];  // worst-case 8x8 FRM line is ~1.43 KB
tofproto::Line L(g_buf, sizeof(g_buf));
tofproto::Clock64 g_clock;

enum class State : uint8_t { WaitMonitor, Error, Idle, Ranging };
State g_state = State::WaitMonitor;
const char* stateName(State s) {
    switch (s) {
        case State::WaitMonitor:
            return "WAIT_MONITOR";
        case State::Error:
            return "ERROR";
        case State::Idle:
            return "IDLE";
        case State::Ranging:
            return "RANGING";
    }
    return "?";
}

uint32_t g_frames = 0;
uint32_t g_read_fail = 0;
uint32_t g_timeouts = 0;
uint32_t g_emit_overflow = 0;
uint8_t g_stalls = 0;
uint64_t g_last_frame_us = 0;
uint64_t g_last_hb_us = 0;
uint64_t g_last_cmd_us = 0;
uint64_t g_ranging_start_us = 0;
const char* g_last_error = "none";

uint64_t nowUs() {
    return g_clock.extend(static_cast<uint32_t>(micros()));
}

uint8_t platWrite(void* h, uint16_t reg, uint8_t* p, uint32_t n) {
    return static_cast<tofplat::RegisterBus<TwoWire>*>(h)->write(reg, p, n);
}
uint8_t platRead(void* h, uint16_t reg, uint8_t* p, uint32_t n) {
    return static_cast<tofplat::RegisterBus<TwoWire>*>(h)->read(reg, p, n);
}
uint8_t platWait(void*, uint32_t ms) {
    delay(ms);
    return 0;
}

void emit() {
    const size_t n = L.finish();
    if (n == 0) {
        ++g_emit_overflow;
        return;
    }
    Monitor.write(reinterpret_cast<const uint8_t*>(g_buf), n);
}

void evt(char level, const char* code) {
    L.begin("EVT");
    L.u64(nowUs());
    const char lv[2] = {level, '\0'};
    L.str(lv);
    L.str(code);
}

void addI2cStats() {
    const tofplat::Stats& s = g_bus.stats();
    L.kv("i2c_tx", s.transactions);
    L.kv("i2c_retries", s.retries);
    L.kv("i2c_failures", s.failures);
    L.kv("i2c_last_fail_reg", s.last_fail_reg);
    L.kv("i2c_last_fail_stage", s.last_fail_stage);
}

void emitHello() {
    L.begin("HELLO");
    L.u64(nowUs());
    L.kv("proto", tofproto::kProtocolVersion);
    L.kvs("sketch", "tof_unoq_vl53l8cx " SKETCH_VERSION);
    L.kvs("uld", VL53L8CX_API_REVISION);
    L.kvs("board", "UNO_Q_STM32U585");
    L.kvs("bus", "Wire1_I2C4_SDA_PD13_SCL_PD12");
    L.kv("addr7", kAddr7);
    L.kv("i2c_hz", kI2cHz);
    L.kvs("time", "mcu_micros_extended_64bit");
    emit();
}

void emitHeartbeat() {
    L.begin("HB");
    L.u64(nowUs());
    L.kvs("state", stateName(g_state));
    L.kv("frames", g_frames);
    L.kv("read_fail", g_read_fail);
    L.kv("timeouts", g_timeouts);
    L.kv("emit_overflow", g_emit_overflow);
    L.kvs("last_error", g_last_error);
    addI2cStats();
    emit();
}

void fail(const char* code, uint8_t uld_status) {
    g_last_error = code;
    g_state = State::Error;
    evt('E', code);
    L.kv("uld_status", uld_status);
    addI2cStats();
    emit();
}

void scanBus() {
    // One-byte read probe at every 7-bit address. Harmless for the VL53L8CX;
    // a read can have side effects on some other device types, but the Qwiic
    // bus should hold nothing else during this bring-up.
    char found[200];
    size_t len = 0;
    found[0] = '\0';
    uint32_t count = 0;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        while (tofWire.available() > 0)
            tofWire.read();
        const size_t got = tofWire.requestFrom(a, static_cast<size_t>(1));
        while (tofWire.available() > 0)
            tofWire.read();
        if (got == 1 && len + 6 < sizeof(found)) {
            static const char hex[] = "0123456789abcdef";
            if (count) found[len++] = ' ';
            found[len++] = '0';
            found[len++] = 'x';
            found[len++] = hex[a >> 4];
            found[len++] = hex[a & 0xF];
            found[len] = '\0';
            ++count;
        }
    }
    evt(count ? 'I' : 'E', "SCAN");
    L.kv("count", count);
    L.kvs("addrs", count ? found : "none");
    emit();
}

bool configure() {
    uint8_t st = 0;
    st |= vl53l8cx_set_resolution(&g_dev, g_resolution);
    st |= vl53l8cx_set_ranging_frequency_hz(&g_dev, g_hz);
    st |= vl53l8cx_set_ranging_mode(&g_dev, VL53L8CX_RANGING_MODE_CONTINUOUS);
    if (st != VL53L8CX_STATUS_OK) {
        fail("CFG_FAIL", st);
        return false;
    }
    // Report what the sensor says it is doing, not what was requested.
    uint8_t res = 0, hz = 0, sharpener = 0, order = 0, mode = 0;
    uint32_t integration = 0;
    st |= vl53l8cx_get_resolution(&g_dev, &res);
    st |= vl53l8cx_get_ranging_frequency_hz(&g_dev, &hz);
    st |= vl53l8cx_get_integration_time_ms(&g_dev, &integration);
    st |= vl53l8cx_get_sharpener_percent(&g_dev, &sharpener);
    st |= vl53l8cx_get_target_order(&g_dev, &order);
    st |= vl53l8cx_get_ranging_mode(&g_dev, &mode);
    if (st != VL53L8CX_STATUS_OK) {
        fail("CFG_READBACK_FAIL", st);
        return false;
    }
    L.begin("CFG");
    L.u64(nowUs());
    L.kv("zones", res);
    L.kv("hz", hz);
    L.kvs("mode", mode == VL53L8CX_RANGING_MODE_CONTINUOUS   ? "continuous"
                  : mode == VL53L8CX_RANGING_MODE_AUTONOMOUS ? "autonomous"
                                                             : "unknown");
    L.kv("integration_ms", integration);
    L.kv("sharpener_pct", sharpener);
    L.kvs("target_order", order == VL53L8CX_TARGET_ORDER_CLOSEST     ? "closest"
                          : order == VL53L8CX_TARGET_ORDER_STRONGEST ? "strongest"
                                                                     : "unknown");
    L.kv("targets_per_zone", VL53L8CX_NB_TARGET_PER_ZONE);
    L.kvs("zone_fields", "distance_mm:target_status:nb_target_detected:range_sigma_mm");
    L.kvs("zone_order", "uld_index_raw_no_flip");
    L.kvs("calibration", "none_applied_by_this_sketch");
    emit();
    return true;
}

bool startRanging() {
    const uint8_t st = vl53l8cx_start_ranging(&g_dev);
    if (st != VL53L8CX_STATUS_OK) {
        fail("RANGING_START_FAIL", st);
        return false;
    }
    g_state = State::Ranging;
    g_stalls = 0;
    g_ranging_start_us = nowUs();
    g_last_frame_us = g_ranging_start_us;
    evt('I', "RANGING_START");
    emit();
    return true;
}

void bringUp() {
    g_last_error = "none";
    g_bus.resetStats();

    tofWire.begin();
    tofWire.setClock(kI2cHz);
    evt('I', "BUS_BEGIN");
    L.kvs("bus", "Wire1_I2C4_SDA_PD13_SCL_PD12");
    L.kv("hz", kI2cHz);
    emit();

    scanBus();

    if (!g_bus.probe()) {
        g_last_error = "NO_ACK";
        g_state = State::Error;
        evt('E', "NO_ACK");
        L.kv("addr7", kAddr7);
        L.kvs("hint", "check_VIN_GND_SDA_SCL_wiring_and_SPI_I2C_jumper_to_GND");
        emit();
        return;
    }
    evt('I', "ACK");
    L.kv("addr7", kAddr7);
    L.kvs("note", "ack_only_does_not_prove_ranging");
    emit();

    // Same register sequence as ULD vl53l8cx_is_alive(), but reports the raw bytes.
    uint8_t id[2] = {0, 0};
    uint8_t page0 = 0x00, page2 = 0x02;
    uint8_t st = g_bus.write(0x7FFF, &page0, 1);
    st |= g_bus.read(0x0000, id, 2);
    st |= g_bus.write(0x7FFF, &page2, 1);
    const bool alive = (st == 0 && id[0] == 0xF0 && id[1] == 0x0C);
    evt(alive ? 'I' : 'E', alive ? "ID_OK" : "ID_FAIL");
    L.kv("device_id", id[0]);
    L.kv("revision_id", id[1]);
    L.kv("expect_device_id", 0xF0);
    L.kv("expect_revision_id", 0x0C);
    L.kv("comms_status", st);
    addI2cStats();
    emit();
    if (!alive) {
        g_last_error = "ID_FAIL";
        g_state = State::Error;
        return;
    }

    memset(&g_dev, 0, sizeof(g_dev));
    g_dev.platform.address = static_cast<uint16_t>(kAddr7 << 1);
    g_dev.platform.Write = platWrite;
    g_dev.platform.Read = platRead;
    g_dev.platform.Wait = platWait;
    g_dev.platform.handle = &g_bus;

    evt('I', "INIT_START");
    L.kvs("note", "uploading_about_84KB_firmware_takes_seconds");
    emit();
    const uint64_t t0 = nowUs();
    st = vl53l8cx_init(&g_dev);
    const uint64_t dt_ms = (nowUs() - t0) / 1000;
    if (st != VL53L8CX_STATUS_OK) {
        fail("INIT_FAIL", st);
        return;
    }
    evt('I', "INIT_OK");
    L.kv("ms", dt_ms);
    addI2cStats();
    emit();

    if (!configure()) return;
    g_state = State::Idle;
    startRanging();
}

void emitFrame(uint64_t t_ready, uint64_t t_done) {
    const uint8_t zones = g_resolution;
    L.begin("FRM");
    L.u64(g_frames);
    L.u64(g_dev.streamcount);
    L.u64(t_ready);
    L.u64(t_done);
    L.u64(zones);
    L.i32(g_results.silicon_temp_degc);
    for (uint8_t z = 0; z < zones; ++z) {
        const uint32_t i = static_cast<uint32_t>(z) * VL53L8CX_NB_TARGET_PER_ZONE;  // first target
        L.zone(g_results.distance_mm[i], g_results.target_status[i],
               g_results.nb_target_detected[z], g_results.range_sigma_mm[i]);
    }
    emit();
}

void serviceRanging() {
    uint8_t ready = 0;
    const uint8_t st = vl53l8cx_check_data_ready(&g_dev, &ready);
    const uint64_t now = nowUs();
    if (st != VL53L8CX_STATUS_OK) {
        ++g_read_fail;
        evt('W', "READY_POLL_FAIL");
        L.kv("uld_status", st);
        addI2cStats();
        emit();
        delay(10);
        return;
    }
    if (!ready) {
        const uint64_t period_us = 1000000ULL / (g_hz ? g_hz : 1);
        const uint64_t limit = (3 * period_us > 1000000ULL) ? 3 * period_us : 1000000ULL;
        if (now - g_last_frame_us > limit) {
            ++g_timeouts;
            ++g_stalls;
            g_last_frame_us = now;
            evt('W', "DATA_TIMEOUT");
            L.kv("stalls", g_stalls);
            emit();
            if (g_stalls >= kStallLimit) {
                vl53l8cx_stop_ranging(&g_dev);
                fail("RANGING_STALLED", 0);
            }
        }
        delay(kReadyPollMs);
        return;
    }
    const uint64_t t_ready = now;
    const uint8_t rs = vl53l8cx_get_ranging_data(&g_dev, &g_results);
    const uint64_t t_done = nowUs();
    g_last_frame_us = t_done;
    if (rs != VL53L8CX_STATUS_OK) {
        ++g_read_fail;
        evt('E', "READ_FAIL");
        L.kv("uld_status", rs);
        addI2cStats();
        emit();
        return;
    }
    g_stalls = 0;
    emitFrame(t_ready, t_done);
    ++g_frames;
}

void handleCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;
    evt('I', "CMD");
    L.kvs("cmd", cmd.c_str());
    emit();
    if (cmd == "STATUS") {
        emitHello();
        if (g_state == State::Idle || g_state == State::Ranging) configure_report_only();
        emitHeartbeat();
    } else if (cmd == "STOP") {
        if (g_state == State::Ranging) {
            const uint8_t st = vl53l8cx_stop_ranging(&g_dev);
            g_state = State::Idle;
            evt(st ? 'E' : 'I', "RANGING_STOP");
            L.kv("uld_status", st);
            emit();
        }
    } else if (cmd == "START") {
        if (g_state == State::Idle) {
            if (configure()) startRanging();
        } else if (g_state == State::Error) {
            bringUp();
        }
    } else if (cmd.startsWith("RES ") || cmd.startsWith("HZ ")) {
        if (g_state == State::Ranging) {
            evt('W', "BUSY");
            L.kvs("hint", "send_STOP_first");
            emit();
            return;
        }
        const long v = cmd.substring(cmd.indexOf(' ') + 1).toInt();
        if (cmd.startsWith("RES ")) {
            if (v == 4)
                g_resolution = VL53L8CX_RESOLUTION_4X4;
            else if (v == 8)
                g_resolution = VL53L8CX_RESOLUTION_8X8;
        } else if (v > 0 && v <= 60) {
            g_hz = static_cast<uint8_t>(v);
        }
        if (g_state == State::Idle) configure();
    } else if (cmd == "SCAN") {
        if (g_state != State::Ranging) scanBus();
    } else {
        evt('W', "UNKNOWN_CMD");
        emit();
    }
}

// CFG record without changing the sensor (used by STATUS while ranging).
void configure_report_only() {
    uint8_t res = 0, hz = 0;
    uint32_t integration = 0;
    uint8_t st = vl53l8cx_get_resolution(&g_dev, &res);
    st |= vl53l8cx_get_ranging_frequency_hz(&g_dev, &hz);
    st |= vl53l8cx_get_integration_time_ms(&g_dev, &integration);
    L.begin("CFG");
    L.u64(nowUs());
    L.kv("zones", res);
    L.kv("hz", hz);
    L.kv("integration_ms", integration);
    L.kv("readback_status", st);
    L.kvs("note", "status_snapshot");
    emit();
}

String g_cmd;

// One available() call (one RPC) per poll, then drain what it fetched.
// Stream::readStringUntil() is not used: BridgeMonitor::read() never fetches
// new data, so a timed read would split a command across polls.
void pollCommands() {
    int pending = Monitor.available();
    uint8_t tmp[64];
    while (pending > 0) {
        const int got =
            Monitor.read(tmp, pending < 64 ? static_cast<size_t>(pending) : sizeof(tmp));
        if (got <= 0) break;
        pending -= got;
        for (int i = 0; i < got; ++i) {
            const char c = static_cast<char>(tmp[i]);
            if (c == '\n' || c == '\r') {
                if (g_cmd.length() > 0) handleCommand(g_cmd);
                g_cmd = "";
            } else if (g_cmd.length() < 64) {
                g_cmd += c;
            }
        }
    }
}

}  // namespace

void setup() {
    Bridge.begin();
    Monitor.begin();
}

void loop() {
    const uint64_t now = nowUs();

    if (g_state == State::WaitMonitor) {
        if (Monitor) {  // a client is connected to 127.0.0.1:7500 on Linux
            emitHello();
            bringUp();
            g_last_hb_us = nowUs();
        } else {
            delay(500);
        }
        return;
    }

    if (now - g_last_cmd_us >= kCommandPollMs * 1000ULL) {
        g_last_cmd_us = now;
        pollCommands();
    }
    if (now - g_last_hb_us >= kHeartbeatMs * 1000ULL) {
        g_last_hb_us = now;
        emitHeartbeat();
    }
    if (g_state == State::Ranging) {
        serviceRanging();
    } else {
        delay(10);
    }
}
