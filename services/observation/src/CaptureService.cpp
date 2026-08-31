#include "platypus/observation/CaptureService.hpp"

#include <cstdio>
#include <ctime>
#include <fstream>
#include <system_error>

namespace platypus::observation {

namespace fs = std::filesystem;
using hal::Error;
using hal::PixelFormat;

std::string_view imageExtensionFor(PixelFormat format) noexcept {
    switch (format) {
        case PixelFormat::MJPEG:  return ".jpg";
        case PixelFormat::RGB888: return ".ppm";
        case PixelFormat::Gray8:  return ".pgm";
        case PixelFormat::YUYV:   return ".yuyv";
        case PixelFormat::NV12:   return ".nv12";
        case PixelFormat::Unknown: break;
    }
    return ".bin";
}

std::string_view imageKindFor(PixelFormat format) noexcept {
    switch (format) {
        case PixelFormat::MJPEG:  return "image/jpeg";
        case PixelFormat::RGB888: return "image/x-portable-pixmap";
        case PixelFormat::Gray8:  return "image/x-portable-graymap";
        case PixelFormat::YUYV:   return "application/x-raw-yuyv";
        case PixelFormat::NV12:   return "application/x-raw-nv12";
        case PixelFormat::Unknown: break;
    }
    return "application/octet-stream";
}

namespace {

std::string_view pixelFormatName(PixelFormat format) noexcept {
    switch (format) {
        case PixelFormat::Gray8:  return "gray8";
        case PixelFormat::RGB888: return "rgb888";
        case PixelFormat::YUYV:   return "yuyv";
        case PixelFormat::MJPEG:  return "mjpeg";
        case PixelFormat::NV12:   return "nv12";
        case PixelFormat::Unknown: break;
    }
    return "unknown";
}

/// PPM (P6) / PGM (P5) get a header so the file opens in any image viewer;
/// everything else is the frame's bytes verbatim.
bool writeImage(const fs::path& path, const hal::Frame& frame) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    const auto& mode = frame.mode();
    if (mode.format == PixelFormat::RGB888 || mode.format == PixelFormat::Gray8) {
        out << (mode.format == PixelFormat::RGB888 ? "P6\n" : "P5\n")
            << mode.width << ' ' << mode.height << "\n255\n";
    }
    const auto pixels = frame.pixels();
    out.write(reinterpret_cast<const char*>(pixels.data()),
              static_cast<std::streamsize>(pixels.size()));
    return out.good();
}

std::string decimal(unsigned value) { return std::to_string(value); }

}  // namespace

CaptureService::CaptureService(fs::path observationsRoot)
    : root_(std::move(observationsRoot)) {}

std::string CaptureService::nextObservationId() const {
    for (unsigned n = 1; n <= 9999; ++n) {
        char id[16];
        std::snprintf(id, sizeof(id), "scan-%04u", n);
        std::error_code ec;
        if (!fs::exists(root_ / id, ec)) return id;
    }
    return "scan-overflow";  // 10k scans on-device warrants a better scheme
}

std::string CaptureService::currentUtcTimestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

hal::Result<CaptureResult> CaptureService::capture(hal::ICamera& camera,
                                                   const CaptureConfig& config) {
    if (config.observationId.empty() ||
        config.observationId.find_first_of("/\\") != std::string::npos ||
        config.timestampUtc.empty())
        return Error::InvalidArgument;

    const fs::path directory = root_ / config.observationId;
    std::error_code ec;
    if (fs::exists(directory, ec)) return Error::Busy;  // evidence is never overwritten

    // 1. Acquire the frame BEFORE touching the filesystem: a camera failure
    //    must not leave a false observation behind.
    auto frame = camera.capture(config.captureTimeout);
    if (!frame) return frame.error();
    if (frame.value().empty()) return Error::IoFailure;

    const auto& mode = frame.value().mode();
    const std::string imageName =
        std::string("source") + std::string(imageExtensionFor(mode.format));

    // 2. Build and validate the record before writing anything.
    EngineeringObservation record;
    record.observationId = config.observationId;
    record.timestampUtc = config.timestampUtc;
    record.source = config.source;
    record.source.emplace_back("frame_width", decimal(mode.width));
    record.source.emplace_back("frame_height", decimal(mode.height));
    record.source.emplace_back("pixel_format", std::string(pixelFormatName(mode.format)));
    record.artifacts.push_back(
        {"source-image", std::string(imageKindFor(mode.format)), imageName});

    if (!validate(record).empty()) return Error::InvalidArgument;

    // 3. Persist atomically enough for v0.1: create the directory, write both
    //    files, and remove the whole directory on any failure.
    if (!fs::create_directories(directory, ec) || ec) return Error::IoFailure;

    const fs::path imagePath = directory / imageName;
    const fs::path recordPath = directory / "observation.json";

    const auto abort = [&] {
        std::error_code cleanupEc;
        fs::remove_all(directory, cleanupEc);
        return Error::IoFailure;
    };

    if (!writeImage(imagePath, frame.value())) return abort();

    const std::string json = toJson(record);
    {
        std::ofstream out(recordPath, std::ios::binary);
        if (!out) return abort();
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        if (!out.good()) return abort();
    }

    return CaptureResult{directory, imagePath, recordPath, std::move(record)};
}

}  // namespace platypus::observation
