#include <platypus/hal/tof/TofProtocol.hpp>

#include <algorithm>
#include <charconv>

namespace platypus::hal::tof {

namespace {

std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        const auto at = s.find(sep, start);
        out.push_back(s.substr(start, at == std::string_view::npos ? s.npos : at - start));
        if (at == std::string_view::npos) break;
        start = at + 1;
    }
    return out;
}

template <typename T>
bool toNumber(std::string_view s, T& out) {
    if (s.empty()) return false;
    const auto* end = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(s.data(), end, out);
    return ec == std::errc{} && ptr == end;
}

// FRM fields: seq, streamcount, us_ready, us_read_done, zones, temp_c, then
// one "d:s:n:g" per zone.
std::optional<Frame> parseFrame(const std::vector<std::string_view>& f) {
    if (f.size() < 6) return std::nullopt;
    Frame frame;
    std::uint64_t readDone = 0;
    std::int32_t zones = 0;
    if (!toNumber(f[0], frame.seq) || !toNumber(f[1], frame.streamcount) ||
        !toNumber(f[2], frame.mcuUsReady) || !toNumber(f[3], readDone) || !toNumber(f[4], zones) ||
        !toNumber(f[5], frame.temperatureC))
        return std::nullopt;
    if (zones != 16 && zones != 64) return std::nullopt;
    if (f.size() != 6 + static_cast<std::size_t>(zones)) return std::nullopt;
    frame.side = zones == 64 ? 8 : 4;
    frame.zones.reserve(static_cast<std::size_t>(zones));
    for (std::size_t i = 6; i < f.size(); ++i) {
        const auto parts = split(f[i], ':');
        if (parts.size() != 4) return std::nullopt;
        Zone z;
        if (!toNumber(parts[0], z.distanceMm) || !toNumber(parts[1], z.status) ||
            !toNumber(parts[2], z.targets) || !toNumber(parts[3], z.sigmaMm))
            return std::nullopt;
        frame.zones.push_back(z);
    }
    return frame;
}

}  // namespace

std::uint16_t crc16CcittFalse(std::string_view data) noexcept {
    std::uint16_t crc = 0xFFFF;
    for (const char c : data) {
        crc ^= static_cast<std::uint16_t>(static_cast<std::uint8_t>(c) << 8);
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<std::uint16_t>(crc << 1);
    }
    return crc;
}

std::optional<Record> parseLine(std::string_view line) {
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
        line.remove_suffix(1);
    const auto dollar = line.find('$');
    const auto star = line.rfind('*');
    if (dollar == line.npos || star == line.npos || star < dollar || line.size() != star + 5)
        return std::nullopt;
    const auto body = line.substr(dollar + 1, star - dollar - 1);
    std::uint16_t crc = 0;
    const auto hex = line.substr(star + 1, 4);
    const auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), crc, 16);
    if (ec != std::errc{} || ptr != hex.data() + hex.size() || crc != crc16CcittFalse(body))
        return std::nullopt;

    auto parts = split(body, ',');
    if (parts.size() < 2 || parts[0] != "TOF") return std::nullopt;
    Record rec;
    rec.typeName = std::string(parts[1]);
    const std::vector<std::string_view> fields(parts.begin() + 2, parts.end());
    for (const auto f : fields)
        rec.fields.emplace_back(f);

    if (rec.typeName == "FRM") {
        rec.type = RecordType::Frame;
        rec.frame = parseFrame(fields);
        if (!rec.frame) return std::nullopt;
    } else if (rec.typeName == "EVT") {
        rec.type = RecordType::Event;
        if (fields.size() < 3 || fields[1].size() != 1) return std::nullopt;
        rec.level = fields[1][0];
        rec.code = std::string(fields[2]);
    } else if (rec.typeName == "HELLO") {
        rec.type = RecordType::Hello;
    } else if (rec.typeName == "CFG") {
        rec.type = RecordType::Config;
    } else if (rec.typeName == "HB") {
        rec.type = RecordType::Heartbeat;
    }
    return rec;
}

std::vector<std::string> LineAssembler::feed(std::string_view bytes) {
    std::vector<std::string> lines;
    for (const char c : bytes) {
        if (c == '\n') {
            lines.push_back(std::move(pending_));
            pending_.clear();
        } else if (pending_.size() < 4096) {  // a runaway line is dropped by its CRC
            pending_.push_back(c);
        }
    }
    return lines;
}

std::optional<double> centralMedianMm(const Frame& frame) {
    if (frame.side < 2 || frame.zones.size() != static_cast<std::size_t>(frame.side * frame.side))
        return std::nullopt;
    const std::int32_t a = frame.side / 2 - 1, b = frame.side / 2;
    std::vector<double> d;
    for (const auto r : {a, b})
        for (const auto c : {a, b})
            if (const auto& z = frame.at(r, c); isValid(z.status)) d.push_back(z.distanceMm);
    if (d.empty()) return std::nullopt;
    std::sort(d.begin(), d.end());
    const auto n = d.size();
    return n % 2 ? d[n / 2] : 0.5 * (d[n / 2 - 1] + d[n / 2]);
}

std::string_view to_string(Orientation o) noexcept {
    switch (o) {
        case Orientation::Raw:
            return "RAW";
        case Orientation::FlipH:
            return "FLIP H";
        case Orientation::FlipV:
            return "FLIP V";
        case Orientation::Rotate180:
            return "ROT 180";
    }
    return "RAW";
}

Orientation next(Orientation o) noexcept {
    switch (o) {
        case Orientation::Raw:
            return Orientation::FlipH;
        case Orientation::FlipH:
            return Orientation::FlipV;
        case Orientation::FlipV:
            return Orientation::Rotate180;
        case Orientation::Rotate180:
            return Orientation::Raw;
    }
    return Orientation::Raw;
}

std::int32_t zoneIndexAt(std::int32_t row, std::int32_t col, std::int32_t side,
                         Orientation o) noexcept {
    const bool flipH = o == Orientation::FlipH || o == Orientation::Rotate180;
    const bool flipV = o == Orientation::FlipV || o == Orientation::Rotate180;
    const std::int32_t r = flipV ? side - 1 - row : row;
    const std::int32_t c = flipH ? side - 1 - col : col;
    return r * side + c;
}

}  // namespace platypus::hal::tof

#include <platypus/hal/tof/ITofSource.hpp>

namespace platypus::hal::tof {

FakeTofSource::FakeTofSource() {
    frame_.side = 8;
    frame_.seq = 1898;
    frame_.temperatureC = 55;
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col) {
            Zone z;
            z.distanceMm = 214 + 2 * row + col;
            z.status = 5;
            z.targets = 1;
            if (row >= 2 && row <= 4 && col >= 4 && col <= 6) z.distanceMm = 150 + row;
            if (row == 7 && col >= 6) z.status = col == 7 ? 255 : 4;
            frame_.zones.push_back(z);
        }
}

TofSnapshot FakeTofSource::snapshot() const {
    TofSnapshot s;
    if (!running_) return s;
    s.connected = true;
    s.frame = frame_;
    s.frameArrived = std::chrono::steady_clock::now();
    s.hz = 5.0;
    s.frames = 1;
    return s;
}

}  // namespace platypus::hal::tof
