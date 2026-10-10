#include "CsiCamera.hpp"

#include <fcntl.h>
#include <linux/media-bus-format.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>

namespace platypus::unoq {

using hal::Error;
using hal::Frame;
using hal::Result;
using hal::Status;
namespace bayer = hal::bayer;

namespace {

constexpr unsigned kBufferCount = 3;    // 2.5 MB each: well inside the 32 MB CMA pool
constexpr unsigned kWarmupFrames = 15;  // lets auto-exposure settle before the first frame
constexpr const char* kCsid = "msm_csid0";
constexpr const char* kRdi = "msm_vfe0_rdi0";
constexpr const char* kVideo = "msm_vfe0_video0";

int xioctl(int fd, unsigned long request, void* arg) {
    int r;
    do {
        r = ::ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

struct Entity {
    std::uint32_t id = 0;
    std::string name;
    std::uint16_t pads = 0;
    std::uint16_t links = 0;
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
};

int openCamssMedia() {
    for (int n = 0; n < 8; ++n) {
        const std::string path = "/dev/media" + std::to_string(n);
        const int fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        media_device_info info{};
        if (xioctl(fd, MEDIA_IOC_DEVICE_INFO, &info) == 0 &&
            std::strncmp(info.driver, "qcom-camss", sizeof(info.driver)) == 0)
            return fd;
        ::close(fd);
    }
    return -1;
}

std::vector<Entity> listEntities(int fd) {
    std::vector<Entity> out;
    media_entity_desc d{};
    d.id = MEDIA_ENT_ID_FLAG_NEXT;
    while (xioctl(fd, MEDIA_IOC_ENUM_ENTITIES, &d) == 0) {
        Entity e;
        e.id = d.id;
        e.name.assign(d.name, strnlen(d.name, sizeof(d.name)));
        e.pads = d.pads;
        e.links = d.links;
        e.major = d.dev.major;
        e.minor = d.dev.minor;
        out.push_back(e);
        const std::uint32_t id = d.id;
        d = media_entity_desc{};
        d.id = id | MEDIA_ENT_ID_FLAG_NEXT;
    }
    return out;
}

/// Outgoing links of an entity (the kernel lists links where it is the source).
std::vector<media_link_desc> listLinks(int fd, const Entity& e) {
    std::vector<media_pad_desc> pads(e.pads);
    std::vector<media_link_desc> links(e.links);
    media_links_enum le{};
    le.entity = e.id;
    le.pads = pads.empty() ? nullptr : pads.data();
    le.links = links.empty() ? nullptr : links.data();
    if (xioctl(fd, MEDIA_IOC_ENUM_LINKS, &le) != 0) return {};
    return links;
}

bool setLink(int fd, media_link_desc link, bool enable) {
    if (link.flags & MEDIA_LNK_FL_IMMUTABLE) return true;
    link.flags = (link.flags & ~static_cast<std::uint32_t>(MEDIA_LNK_FL_ENABLED)) |
                 (enable ? static_cast<std::uint32_t>(MEDIA_LNK_FL_ENABLED) : 0u);
    return xioctl(fd, MEDIA_IOC_SETUP_LINK, &link) == 0;
}

/// /dev path of a character device, from its sysfs uevent DEVNAME.
std::string devnode(std::uint32_t major, std::uint32_t minor) {
    std::ifstream in("/sys/dev/char/" + std::to_string(major) + ":" + std::to_string(minor) +
                     "/uevent");
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("DEVNAME=", 0) == 0) return "/dev/" + line.substr(8);
    return {};
}

bool setSubdevFormat(const std::string& node, std::uint32_t pad) {
    const int fd = ::open(node.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) return false;
    v4l2_subdev_format f{};
    f.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    f.pad = pad;
    f.format.width = CsiCamera::kRawWidth;
    f.format.height = CsiCamera::kRawHeight;
    f.format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
    f.format.field = V4L2_FIELD_NONE;
    const bool ok = xioctl(fd, VIDIOC_SUBDEV_S_FMT, &f) == 0;
    ::close(fd);
    return ok;
}

const Entity* byName(const std::vector<Entity>& all, const std::string& name) {
    for (const auto& e : all)
        if (e.name == name) return &e;
    return nullptr;
}

const Entity* sensorOf(const std::vector<Entity>& all) {
    for (const auto& e : all)
        if (e.name.rfind("imx219 ", 0) == 0) return &e;
    return nullptr;
}

std::optional<std::int32_t> queryMax(int fd, std::uint32_t id) {
    v4l2_queryctrl q{};
    q.id = id;
    if (xioctl(fd, VIDIOC_QUERYCTRL, &q) != 0) return std::nullopt;
    return q.maximum;
}

std::optional<std::int64_t> getCtrl(int fd, std::uint32_t id) {
    v4l2_ext_control c{};
    c.id = id;
    v4l2_ext_controls cs{};
    cs.which = V4L2_CTRL_WHICH_CUR_VAL;
    cs.count = 1;
    cs.controls = &c;
    if (xioctl(fd, VIDIOC_G_EXT_CTRLS, &cs) != 0) return std::nullopt;
    return id == V4L2_CID_PIXEL_RATE ? c.value64 : c.value;
}

bool setCtrl(int fd, std::uint32_t id, std::int32_t value) {
    v4l2_control c{};
    c.id = id;
    c.value = value;
    return xioctl(fd, VIDIOC_S_CTRL, &c) == 0;
}

}  // namespace

CsiCamera::CsiCamera() = default;

CsiCamera::~CsiCamera() {
    close();
}

bool CsiCamera::sensorPresent() {
    const int fd = openCamssMedia();
    if (fd < 0) return false;
    const bool present = sensorOf(listEntities(fd)) != nullptr;
    ::close(fd);
    return present;
}

std::string CsiCamera::deviceIdentity() const {
    std::lock_guard lock(stateMutex_);
    if (sensorName_.empty()) return lastError_;
    return sensorName_ + " csi raw10 " + std::to_string(kRawWidth) + "x" +
           std::to_string(kRawHeight) + "->" + std::to_string(kMode.width) + "x" +
           std::to_string(kMode.height) + " exp=" + std::to_string(exposure_.lines) +
           " gain=" + std::to_string(exposure_.gainCode) +
           " dgain=" + std::to_string(exposure_.digitalCode) +
           " line_ns=" + std::to_string(lineTimeNs_);
}

Status CsiCamera::configureGraph() {
    const auto all = listEntities(mediaFd_);
    const Entity* sensor = sensorOf(all);
    const Entity* csid = byName(all, kCsid);
    const Entity* rdi = byName(all, kRdi);
    const Entity* video = byName(all, kVideo);
    if (!sensor) {
        lastError_ = "no imx219 sensor in the camss media graph";
        return Error::NotSupported;
    }
    if (!csid || !rdi || !video) {
        lastError_ = "camss graph lacks msm_csid0 / msm_vfe0_rdi0";
        return Error::NotSupported;
    }

    // The sensor's single, immutable link names the CSI PHY it is wired to.
    std::uint32_t phyId = 0;
    std::string phyName;
    for (const auto& l : listLinks(mediaFd_, *sensor))
        if (l.source.entity == sensor->id) phyId = l.sink.entity;
    for (const auto& e : all)
        if (e.id == phyId) phyName = e.name;
    if (phyName.empty()) {
        lastError_ = "sensor has no CSI PHY link";
        return Error::NotSupported;
    }

    // Route that PHY, and only that PHY, into csid0; csid0 into RDI0.
    for (const auto& e : all) {
        if (e.name.rfind("msm_csiphy", 0) != 0) continue;
        for (const auto& l : listLinks(mediaFd_, e))
            if (l.sink.entity == csid->id && l.sink.index == 0 &&
                !setLink(mediaFd_, l, e.id == phyId)) {
                lastError_ = "cannot route " + e.name + " to " + kCsid;
                return Error::IoFailure;
            }
    }
    bool routed = false;
    for (const auto& l : listLinks(mediaFd_, *csid))
        if (l.sink.entity == rdi->id && l.source.index == 1) routed = setLink(mediaFd_, l, true);
    if (!routed) {
        lastError_ = std::string("cannot route ") + kCsid + " to " + kRdi;
        return Error::IoFailure;
    }

    const std::string sensorNode = devnode(sensor->major, sensor->minor);
    const std::string phyNode = [&] {
        for (const auto& e : all)
            if (e.id == phyId) return devnode(e.major, e.minor);
        return std::string{};
    }();
    if (!setSubdevFormat(sensorNode, 0) || !setSubdevFormat(phyNode, 0) ||
        !setSubdevFormat(devnode(csid->major, csid->minor), 0) ||
        !setSubdevFormat(devnode(rdi->major, rdi->minor), 0)) {
        lastError_ = "cannot set SRGGB10 1640x1232 along the CSI chain";
        return Error::IoFailure;
    }

    videoPath_ = devnode(video->major, video->minor);
    sensorFd_ = ::open(sensorNode.c_str(), O_RDWR | O_CLOEXEC);
    if (videoPath_.empty() || sensorFd_ < 0) {
        lastError_ = "cannot open the sensor or RDI video node";
        return Error::IoFailure;
    }
    {
        std::lock_guard lock(stateMutex_);
        sensorName_ = sensor->name;
    }
    // The binned mode's default blanking runs the sensor near 60 fps, which
    // caps exposure at ~16 ms. Stretch the frame to the mode's declared rate
    // so dim benches get real light before any gain; the preview never draws
    // faster than that anyway.
    const auto pixelRate = getCtrl(sensorFd_, V4L2_CID_PIXEL_RATE);
    const auto hblank = getCtrl(sensorFd_, V4L2_CID_HBLANK);
    if (pixelRate && hblank && *pixelRate > 0) {
        const double line = static_cast<double>(kRawWidth + *hblank) / static_cast<double>(*pixelRate);
        const auto frameLines = std::lround(1.0 / (kMode.fps * line));
        if (frameLines > static_cast<long>(kRawHeight))
            setCtrl(sensorFd_, V4L2_CID_VBLANK, static_cast<std::int32_t>(frameLines - kRawHeight));
        lineTimeNs_ = static_cast<std::uint32_t>(std::lround(line * 1e9));
    }
    // Exposure range follows the frame length; read it after S_FMT and VBLANK.
    if (const auto m = queryMax(sensorFd_, V4L2_CID_EXPOSURE); m && *m > 4)
        limits_.maxLines = static_cast<std::uint32_t>(*m);
    if (const auto g = queryMax(sensorFd_, V4L2_CID_ANALOGUE_GAIN); g && *g > 0)
        limits_.maxGainCode = static_cast<std::uint32_t>(*g);
    // Digital gain only once the analogue range is spent, and at most 4x:
    // past that it amplifies noise faster than it helps the threshold.
    if (const auto d = queryMax(sensorFd_, V4L2_CID_DIGITAL_GAIN); d && *d >= 256)
        limits_.maxDigitalCode = std::min<std::uint32_t>(static_cast<std::uint32_t>(*d), 1024);
    return {};
}

Status CsiCamera::open(const hal::CameraMode&) {
    if (videoFd_ >= 0) return Error::Busy;
    mediaFd_ = openCamssMedia();
    if (mediaFd_ < 0) {
        lastError_ = "no qcom-camss media device";
        return Error::NotSupported;
    }
    if (const auto s = configureGraph(); !s) {
        close();
        return s;
    }

    videoFd_ = ::open(videoPath_.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (videoFd_ < 0) {
        close();
        return Error::IoFailure;
    }
    v4l2_capability cap{};
    if (xioctl(videoFd_, VIDIOC_QUERYCAP, &cap) != 0) {
        close();
        return Error::IoFailure;
    }
    const std::uint32_t caps =
        (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
    mplane_ = (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0;
    const auto bufType = mplane_ ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;

    v4l2_format fmt{};
    fmt.type = bufType;
    if (mplane_) {
        fmt.fmt.pix_mp.width = kRawWidth;
        fmt.fmt.pix_mp.height = kRawHeight;
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_SRGGB10P;
        fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
        fmt.fmt.pix_mp.num_planes = 1;
    } else {
        fmt.fmt.pix.width = kRawWidth;
        fmt.fmt.pix.height = kRawHeight;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10P;
        fmt.fmt.pix.field = V4L2_FIELD_NONE;
    }
    if (xioctl(videoFd_, VIDIOC_S_FMT, &fmt) != 0) {
        lastError_ = "RDI node refused SRGGB10P 1640x1232";
        close();
        return Error::IoFailure;
    }
    const std::uint32_t gotW = mplane_ ? fmt.fmt.pix_mp.width : fmt.fmt.pix.width;
    const std::uint32_t gotH = mplane_ ? fmt.fmt.pix_mp.height : fmt.fmt.pix.height;
    const std::uint32_t gotF = mplane_ ? fmt.fmt.pix_mp.pixelformat : fmt.fmt.pix.pixelformat;
    bytesPerLine_ = mplane_ ? fmt.fmt.pix_mp.plane_fmt[0].bytesperline : fmt.fmt.pix.bytesperline;
    if (gotW != kRawWidth || gotH != kRawHeight || gotF != V4L2_PIX_FMT_SRGGB10P ||
        bytesPerLine_ < kRawWidth / 4 * 5) {
        lastError_ = "RDI node granted a different raw format";
        close();
        return Error::NotSupported;
    }

    v4l2_requestbuffers req{};
    req.count = kBufferCount;
    req.type = bufType;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(videoFd_, VIDIOC_REQBUFS, &req) != 0 || req.count < 2) {
        lastError_ = "cannot allocate raw buffers (CMA?)";
        close();
        return Error::IoFailure;
    }
    for (std::uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        v4l2_plane plane{};
        buf.type = bufType;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (mplane_) {
            buf.m.planes = &plane;
            buf.length = 1;
        }
        if (xioctl(videoFd_, VIDIOC_QUERYBUF, &buf) != 0) {
            close();
            return Error::IoFailure;
        }
        const std::size_t length = mplane_ ? plane.length : buf.length;
        const auto offset = static_cast<off_t>(mplane_ ? plane.m.mem_offset : buf.m.offset);
        void* start = ::mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, videoFd_, offset);
        if (start == MAP_FAILED) {
            close();
            return Error::IoFailure;
        }
        buffers_.push_back({start, length});
        if (xioctl(videoFd_, VIDIOC_QBUF, &buf) != 0) {
            close();
            return Error::IoFailure;
        }
    }

    applyExposure(exposure_);
    int type = static_cast<int>(bufType);
    if (xioctl(videoFd_, VIDIOC_STREAMON, &type) != 0) {
        lastError_ = "STREAMON failed";
        close();
        return Error::IoFailure;
    }
    streaming_ = true;
    for (unsigned i = 0; i < kWarmupFrames; ++i) {
        const auto f = dequeueFrame(std::chrono::milliseconds(500));
        if (!f && f.error() != Error::Timeout) break;
    }
    return {};
}

void CsiCamera::applyExposure(const bayer::Exposure& e) {
    if (sensorFd_ < 0) return;
    setCtrl(sensorFd_, V4L2_CID_EXPOSURE, static_cast<std::int32_t>(e.lines));
    setCtrl(sensorFd_, V4L2_CID_ANALOGUE_GAIN, static_cast<std::int32_t>(e.gainCode));
    if (limits_.maxDigitalCode > 256)
        setCtrl(sensorFd_, V4L2_CID_DIGITAL_GAIN, static_cast<std::int32_t>(e.digitalCode));
    std::lock_guard lock(stateMutex_);
    exposure_ = e;
}

Result<Frame> CsiCamera::dequeueFrame(std::chrono::milliseconds timeout) {
    if (!isOpen()) return Error::NotInitialized;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(videoFd_, &fds);
    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    const int ready = ::select(videoFd_ + 1, &fds, nullptr, nullptr, &tv);
    if (ready == 0) return Error::Timeout;
    if (ready < 0) return Error::IoFailure;

    const auto bufType = mplane_ ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buffer buf{};
    v4l2_plane plane{};
    buf.type = bufType;
    buf.memory = V4L2_MEMORY_MMAP;
    if (mplane_) {
        buf.m.planes = &plane;
        buf.length = 1;
    }
    if (xioctl(videoFd_, VIDIOC_DQBUF, &buf) != 0)
        return errno == EAGAIN ? Error::Timeout : Error::IoFailure;
    if (buf.index >= buffers_.size()) return Error::IoFailure;

    // Unpack straight from the mapped buffer, then hand it back at once.
    const std::size_t used = mplane_ ? plane.bytesused : buf.bytesused;
    const auto* src = static_cast<const std::byte*>(buffers_[buf.index].start);
    const bayer::Raw10Layout layout{kRawWidth, kRawHeight, bytesPerLine_};
    auto raw =
        bayer::unpackRaw10(std::span(src, std::min(used, buffers_[buf.index].length)), layout);
    if (xioctl(videoFd_, VIDIOC_QBUF, &buf) != 0) return Error::IoFailure;
    if (raw.empty()) return Error::IoFailure;

    bayer::RgbParams params;
    params.gains = wbGains_;
    bayer::CellStats stats;
    const auto rgb = bayer::binnedRgb(raw, kRawWidth, kRawHeight, params, &stats);

    // White balance follows the scene slowly; exposure steps when needed.
    const auto target = bayer::grayWorldGains(stats);
    for (int c = 0; c < 3; ++c)
        wbGains_[static_cast<std::size_t>(c)] = 0.8f * wbGains_[static_cast<std::size_t>(c)] +
                                                0.2f * target[static_cast<std::size_t>(c)];
    if (autoExposure_) {
        bayer::Exposure current;
        {
            std::lock_guard lock(stateMutex_);
            current = exposure_;
        }
        const auto next = bayer::AutoExposure(limits_).next(stats, current);
        if (next.lines != current.lines || next.gainCode != current.gainCode ||
            next.digitalCode != current.digitalCode)
            applyExposure(next);
    }

    auto pixels = std::make_shared<std::vector<std::byte>>(rgb.size());
    std::memcpy(pixels->data(), rgb.data(), rgb.size());
    return Frame(kMode, std::move(pixels), std::chrono::steady_clock::now());
}

Status CsiCamera::setControls(const hal::CameraControls& controls) {
    // Manual exposure needs the sensor line time, not modelled yet; auto only.
    if (controls.exposureMs || controls.gain) return Error::NotSupported;
    autoExposure_ = true;
    return {};
}

Result<Frame> CsiCamera::capture(std::chrono::milliseconds timeout) {
    if (streamRunning_) return Error::Busy;
    return dequeueFrame(timeout);
}

Status CsiCamera::startStream(std::function<void(const Frame&)> onFrame) {
    if (!isOpen()) return Error::NotInitialized;
    if (streamRunning_) return Error::Busy;
    streamRunning_ = true;
    streamThread_ = std::thread([this, onFrame = std::move(onFrame)] {
        while (streamRunning_) {
            auto frame = dequeueFrame(std::chrono::milliseconds(500));
            if (frame)
                onFrame(frame.value());
            else if (frame.error() != Error::Timeout)
                break;
        }
    });
    return {};
}

Status CsiCamera::stopStream() {
    streamRunning_ = false;
    if (streamThread_.joinable()) streamThread_.join();
    return {};
}

void CsiCamera::releaseBuffers() {
    for (auto& b : buffers_)
        if (b.start && b.start != MAP_FAILED) ::munmap(b.start, b.length);
    buffers_.clear();
}

Status CsiCamera::close() {
    stopStream();
    if (videoFd_ >= 0) {
        if (streaming_) {
            int type = static_cast<int>(mplane_ ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
                                                : V4L2_BUF_TYPE_VIDEO_CAPTURE);
            xioctl(videoFd_, VIDIOC_STREAMOFF, &type);
            streaming_ = false;
        }
        releaseBuffers();
        ::close(videoFd_);
        videoFd_ = -1;
    }
    if (sensorFd_ >= 0) {
        ::close(sensorFd_);
        sensorFd_ = -1;
    }
    if (mediaFd_ >= 0) {
        ::close(mediaFd_);
        mediaFd_ = -1;
    }
    return {};
}

}  // namespace platypus::unoq
