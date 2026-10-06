// scout_measure — run the Scout analyzer on one image file and print JSON.
//
//   scout_measure IMAGE.pgm|IMAGE.ppm [--reference-mm MM]
//
// Reads binary PGM (P5, 8-bit) or PPM (P6, 8-bit) and calls
// vision::analyzeFrame exactly as the capture path does. Prints one JSON
// object on stdout: {"ok":true, ...measurement...} or
// {"ok":false,"error":"<AnalyzeError>"}. Exit 0 whenever the image was read
// (a refused scene is a result, not a tool failure); 2 on usage or I/O errors.
//
// Used by tools/camera_characterize for measurement repeatability. It adds no
// measurement logic of its own.
#include <platypus/vision/ScoutAnalyzer.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using namespace platypus;

bool readToken(std::istream& in, std::string& out) {
    out.clear();
    char c = 0;
    while (in.get(c)) {
        if (c == '#') {
            std::string skip;
            std::getline(in, skip);
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!out.empty()) return true;
            continue;
        }
        out.push_back(c);
    }
    return !out.empty();
}

bool loadPnm(const char* path, hal::Frame& frame, std::string& why) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        why = "cannot open";
        return false;
    }
    std::string magic, ws, hs, maxs;
    if (!readToken(in, magic) || !readToken(in, ws) || !readToken(in, hs) || !readToken(in, maxs)) {
        why = "truncated header";
        return false;
    }
    const int channels = magic == "P5" ? 1 : magic == "P6" ? 3 : 0;
    if (channels == 0 || maxs != "255") {
        why = "only 8-bit binary P5/P6 is supported";
        return false;
    }
    const long w = std::strtol(ws.c_str(), nullptr, 10);
    const long h = std::strtol(hs.c_str(), nullptr, 10);
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535) {
        why = "bad dimensions";
        return false;
    }
    auto data =
        std::make_shared<std::vector<std::byte>>(static_cast<std::size_t>(w * h * channels));
    in.read(reinterpret_cast<char*>(data->data()), static_cast<std::streamsize>(data->size()));
    if (in.gcount() != static_cast<std::streamsize>(data->size())) {
        why = "truncated pixel data";
        return false;
    }
    hal::CameraMode mode;
    mode.width = static_cast<std::uint16_t>(w);
    mode.height = static_cast<std::uint16_t>(h);
    mode.format = channels == 1 ? hal::PixelFormat::Gray8 : hal::PixelFormat::RGB888;
    frame = hal::Frame(mode, std::move(data), std::chrono::steady_clock::time_point{});
    return true;
}

void printBlob(const char* name, const vision::BlobStats& b, bool comma) {
    std::printf(
        "\"%s\":{\"min_x\":%d,\"min_y\":%d,\"max_x\":%d,\"max_y\":%d,\"area_px\":%zu,"
        "\"centroid_x\":%.6f,\"centroid_y\":%.6f,\"fill_ratio\":%.6f,\"hole_count\":%zu,"
        "\"major_axis_angle_rad\":%.6f,\"length_px\":%.6f,\"width_px\":%.6f,"
        "\"touches_border\":%s}%s",
        name, b.minX, b.minY, b.maxX, b.maxY, b.areaPx, b.centroidX, b.centroidY, b.fillRatio,
        b.holeCount, b.majorAxisAngleRad, b.lengthPx, b.widthPx, b.touchesBorder ? "true" : "false",
        comma ? "," : "");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        std::fprintf(stderr, "usage: scout_measure IMAGE.pgm|IMAGE.ppm [--reference-mm MM]\n");
        return 2;
    }
    vision::CalibrationSpec spec;
    if (argc == 4) {
        if (std::string(argv[2]) != "--reference-mm") {
            std::fprintf(stderr, "usage: scout_measure IMAGE.pgm|IMAGE.ppm [--reference-mm MM]\n");
            return 2;
        }
        spec.referenceSideMm = std::strtod(argv[3], nullptr);
        if (!(spec.referenceSideMm > 0.0)) {
            std::fprintf(stderr, "error: --reference-mm must be positive\n");
            return 2;
        }
    }

    hal::Frame frame;
    std::string why;
    if (!loadPnm(argv[1], frame, why)) {
        std::fprintf(stderr, "error: %s: %s\n", argv[1], why.c_str());
        return 2;
    }

    const auto outcome = vision::analyzeFrame(frame, spec);
    if (!outcome.ok()) {
        const auto e = vision::to_string(outcome.error);
        std::printf("{\"ok\":false,\"error\":\"%.*s\",\"reference_mm\":%.6f}\n",
                    static_cast<int>(e.size()), e.data(), spec.referenceSideMm);
        return 0;
    }
    const auto& a = *outcome.analysis;
    std::printf(
        "{\"ok\":true,\"reference_mm\":%.6f,\"threshold\":%u,\"mm_per_pixel\":%.9f,"
        "\"subject_length_mm\":%.6f,\"subject_width_mm\":%.6f,",
        spec.referenceSideMm, static_cast<unsigned>(a.binarizationThreshold), a.mmPerPixel,
        a.subjectLengthMm, a.subjectWidthMm);
    printBlob("reference", a.reference, true);
    printBlob("subject", a.subject, false);
    std::printf("}\n");
    return 0;
}
