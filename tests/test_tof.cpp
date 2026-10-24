// VL53L8CX line protocol and the kiosk's depth view helpers.
//
// The FRM/EVT/HB lines are verbatim records from the 2026-10-18 bench run
// (UNO Q + Pololu #3419 aimed at a wall ~8 in away, run E-wall-8in-a).
#include "KioskCore.hpp"

#include <platypus/hal/tof/TofProtocol.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace platypus;
namespace tof = hal::tof;

const std::string kFrameLine =
    "$TOF,FRM,1898,171,481566801,481596902,64,55,208:5:1:0,210:5:1:1,211:5:1:1,213:5:1:1,216:5:1:"
    "1,217:5:1:1,219:5:1:1,220:5:1:0,210:5:1:1,212:5:1:1,212:5:1:1,213:5:1:1,217:5:1:1,217:5:1:1,"
    "221:5:1:1,222:5:1:0,210:5:1:1,213:5:1:1,212:5:1:1,216:5:1:1,217:5:1:1,220:5:1:1,221:5:1:1,"
    "225:5:1:1,211:5:1:1,214:5:1:1,215:5:1:1,216:5:1:1,219:5:1:1,219:5:1:1,223:5:1:1,224:5:1:1,"
    "213:5:1:1,214:5:1:1,216:5:1:1,219:5:1:1,220:5:1:1,223:5:1:1,223:5:1:1,228:5:1:1,215:5:1:1,"
    "216:5:1:1,219:5:1:1,219:5:1:1,222:5:1:1,221:5:1:1,227:5:1:1,227:5:1:1,216:5:1:0,219:5:1:1,"
    "220:5:1:1,222:5:1:1,223:5:1:1,225:5:1:1,228:5:1:1,232:5:1:1,221:5:1:0,220:5:1:0,224:5:1:1,"
    "223:5:1:1,227:5:1:1,227:5:1:1,233:5:1:1,230:5:1:0*F7D8";

void test_parse_recorded_lines() {
    const auto rec = tof::parseLine(kFrameLine + "\n");
    assert(rec && rec->type == tof::RecordType::Frame && rec->frame);
    const auto& f = *rec->frame;
    assert(f.seq == 1898 && f.streamcount == 171 && f.mcuUsReady == 481566801);
    assert(f.side == 8 && f.zones.size() == 64 && f.temperatureC == 55);
    assert(f.zones[0].distanceMm == 208 && f.zones[0].status == 5 && f.zones[0].sigmaMm == 0);
    assert(f.zones[63].distanceMm == 230);
    // Central 2x2 (zones 27, 28, 35, 36) = 216, 219, 219, 220.
    const auto centre = tof::centralMedianMm(f);
    assert(centre && std::abs(*centre - 219.0) < 1e-9);

    const auto evt = tof::parseLine("$TOF,EVT,471925718,I,CMD,cmd=STATUS*8FE9");
    assert(evt && evt->type == tof::RecordType::Event && evt->level == 'I' && evt->code == "CMD");
    const auto hb = tof::parseLine(
        "$TOF,HB,471960987,state=RANGING,frames=1850,read_fail=0,timeouts=0,emit_overflow=0,last_"
        "error=none,i2c_tx=187171,i2c_retries=0,i2c_failures=0,i2c_last_fail_reg=0,i2c_last_fail_"
        "stage=0*12C0");
    assert(hb && hb->type == tof::RecordType::Heartbeat);
    std::puts("test_tof_parse_recorded_lines: OK");
}

void test_reject_damaged_lines() {
    std::string bad = kFrameLine;
    bad[20] = bad[20] == '1' ? '2' : '1';  // one changed digit
    assert(!tof::parseLine(bad));
    assert(!tof::parseLine(kFrameLine.substr(0, kFrameLine.size() - 1)));  // truncated CRC
    assert(!tof::parseLine(kFrameLine.substr(0, 300)));                    // split mid-record
    assert(!tof::parseLine("garbage"));
    assert(!tof::parseLine(""));
    std::puts("test_tof_reject_damaged_lines: OK");
}

void test_assembler_rejoins_split_lines() {
    tof::LineAssembler a;
    const std::string stream = kFrameLine + "\n$TOF,EVT,471925718,I,CMD,cmd=STATUS*8FE9\n";
    std::vector<std::string> lines;
    for (std::size_t i = 0; i < stream.size(); i += 37) {  // arbitrary chunking
        auto got = a.feed(std::string_view(stream).substr(i, 37));
        lines.insert(lines.end(), got.begin(), got.end());
    }
    assert(lines.size() == 2 && lines[0] == kFrameLine);
    assert(tof::parseLine(lines[1])->code == "CMD");
    std::puts("test_tof_assembler: OK");
}

void test_orientation_mapping() {
    using tof::Orientation;
    assert(tof::zoneIndexAt(0, 0, 8, Orientation::Raw) == 0);
    assert(tof::zoneIndexAt(0, 0, 8, Orientation::FlipH) == 7);
    assert(tof::zoneIndexAt(0, 0, 8, Orientation::FlipV) == 56);
    assert(tof::zoneIndexAt(0, 0, 8, Orientation::Rotate180) == 63);
    assert(tof::zoneIndexAt(2, 5, 8, Orientation::Rotate180) == 5 * 8 + 2);
    Orientation o = Orientation::Raw;
    for (int i = 0; i < 4; ++i)
        o = tof::next(o);
    assert(o == Orientation::Raw);
    std::puts("test_tof_orientation: OK");
}

void test_depth_view_helpers() {
    // ToF 45 deg inside a 75 x 60 deg camera: tan(22.5)/tan(37.5), tan(22.5)/tan(30).
    const auto fp = kiosk::tofFootprint({75.0, 60.0, 45.0});
    assert(std::abs(fp.w - 0.5398) < 1e-3 && std::abs(fp.h - 0.7174) < 1e-3);

    const auto frame = *tof::parseLine(kFrameLine)->frame;
    const auto range = kiosk::depthRange(frame);
    // The wall spans 208..233 mm: under 40 mm, so widened about its middle.
    assert(range && std::abs(range->nearMm - 200.5) < 1e-9 &&
           std::abs(range->farMm - 240.5) < 1e-9);
    // A wider scene keeps its own extremes.
    tof::Frame deep = frame;
    deep.zones[0].distanceMm = 150;
    const auto dr = kiosk::depthRange(deep);
    assert(dr && dr->nearMm == 150 && dr->farMm == 233);
    // A flat target within 40 mm is widened, not stretched into a rainbow.
    tof::Frame flat = frame;
    for (auto& z : flat.zones)
        z.distanceMm = 500;
    const auto fr = kiosk::depthRange(flat);
    assert(fr && std::abs(fr->farMm - fr->nearMm - 40.0) < 1e-9);
    // No valid zone, no range.
    for (auto& z : flat.zones)
        z.status = 255;
    assert(!kiosk::depthRange(flat));

    const auto nearC = kiosk::depthColour(0, {0, 100});
    const auto farC = kiosk::depthColour(100, {0, 100});
    assert(nearC.r > farC.r && nearC.g > farC.g);  // near is bright yellow, far dark purple

    // Overlay: outside the footprint untouched, a valid zone tinted, an
    // invalid zone darkened.
    const std::int32_t w = 100, h = 100;
    std::vector<std::uint8_t> grey(static_cast<std::size_t>(w * h * 3), 120);
    tof::Frame f4;
    f4.side = 4;
    f4.zones.assign(16, tof::Zone{300, 5, 1, 0});
    f4.zones[0].status = 4;
    const auto out =
        kiosk::fuseDepth(grey, w, h, f4, tof::Orientation::Raw, {0.8, 0.8}, {200, 400});
    const auto px = [&](int x, int y) { return &out[static_cast<std::size_t>((y * w + x) * 3)]; };
    assert(px(2, 2)[0] == 120 && px(2, 2)[1] == 120);      // outside
    assert(px(20, 20)[0] == 40);                           // zone 0: invalid, darkened
    assert(px(60, 60)[0] != 120 || px(60, 60)[2] != 120);  // valid: tinted
    std::puts("test_tof_depth_view: OK");
}

}  // namespace

void test_tof() {
    test_parse_recorded_lines();
    test_reject_damaged_lines();
    test_assembler_rejoins_split_lines();
    test_orientation_mapping();
    test_depth_view_helpers();
}
