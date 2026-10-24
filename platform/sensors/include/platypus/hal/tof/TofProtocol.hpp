// PlatypusOS HAL — the VL53L8CX line protocol, consumer side.
//
// The UNO Q MCU sketch (firmware/tof_unoq_vl53l8cx, PR #49) streams one ASCII
// record per line through the Arduino Bridge Monitor (arduino-router, TCP
// 127.0.0.1:7500):
//
//     $TOF,<TYPE>,<field>,...*CCCC\n
//
// CCCC is CRC-16/CCITT-FALSE of the bytes between '$' and '*'. The Monitor path
// can split or drop output, so a line whose CRC fails is rejected, never
// repaired. Field order is the contract in the sketch's tof_proto.h and
// tools/tof_unoq/tofproto.py; this is a third reader of it, kept to what a
// display needs. Nothing here corrects or calibrates a distance.
//
// A ToF frame is a grid of zones, each with its own validity, so it is its own
// type rather than an ISensor float vector: dropping the per-zone status would
// let an invalid zone pass as a distance.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace platypus::hal::tof {

/// One zone as the sensor reported it.
struct Zone {
    std::int32_t distanceMm = 0;
    std::uint16_t status = 255;  ///< ST UM3109 Table 4 target status
    std::uint16_t targets = 0;   ///< nb_target_detected
    std::uint16_t sigmaMm = 0;   ///< range_sigma_mm
};

/// ST UM3109 Rev 7, note under Table 4: status 5 is 100 % valid; 6 and 9 are
/// 50 % confidence; everything else is below that. Displays use only 5.
[[nodiscard]] constexpr bool isValid(std::uint16_t status) noexcept {
    return status == 5;
}

/// One FRM record: zones in the sensor's raw ULD index order (row-major,
/// side x side, not flipped).
struct Frame {
    std::uint64_t seq = 0;
    std::uint64_t streamcount = 0;
    std::uint64_t mcuUsReady = 0;  ///< MCU time the frame was ready, microseconds
    std::int32_t side = 0;         ///< 4 or 8
    std::int32_t temperatureC = 0;
    std::vector<Zone> zones;

    [[nodiscard]] const Zone& at(std::int32_t row, std::int32_t col) const {
        return zones[static_cast<std::size_t>(row * side + col)];
    }
};

/// Median distance of the valid zones in the central 2x2 (the analyzer's
/// "central zones"); nullopt when none of them is valid.
[[nodiscard]] std::optional<double> centralMedianMm(const Frame& frame);

enum class RecordType { Hello, Event, Config, Heartbeat, Frame, Other };

struct Record {
    RecordType type = RecordType::Other;
    std::string typeName;
    std::vector<std::string> fields;  ///< after the type, CRC stripped
    std::optional<Frame> frame;       ///< FRM only
    char level = 0;                   ///< EVT only: 'I', 'W' or 'E'
    std::string code;                 ///< EVT only, e.g. "INIT_OK"
};

[[nodiscard]] std::uint16_t crc16CcittFalse(std::string_view data) noexcept;

/// Parses one line (with or without the trailing newline). nullopt means the
/// line is not a well-formed, CRC-correct record and must be counted as a
/// reject.
[[nodiscard]] std::optional<Record> parseLine(std::string_view line);

/// Reassembles lines from a byte stream that may split them anywhere.
class LineAssembler {
   public:
    /// Appends bytes; returns every line completed by them, without '\n'.
    std::vector<std::string> feed(std::string_view bytes);

   private:
    std::string pending_;
};

/// How the sensor's zone grid maps onto the scene. The receiver lens images
/// the scene through the SPAD array, and the carrier can be mounted either
/// way up, so the mapping is an operator setting until a fixture proves it.
enum class Orientation { Raw, FlipH, FlipV, Rotate180 };

[[nodiscard]] std::string_view to_string(Orientation o) noexcept;
[[nodiscard]] Orientation next(Orientation o) noexcept;

/// Zone index shown at display cell (row, col) for this orientation.
[[nodiscard]] std::int32_t zoneIndexAt(std::int32_t row, std::int32_t col, std::int32_t side,
                                       Orientation o) noexcept;

}  // namespace platypus::hal::tof
