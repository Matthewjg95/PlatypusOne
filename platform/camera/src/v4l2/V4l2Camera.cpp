#include "V4l2Camera.hpp"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace platypus::unoq {

using hal::CameraMode;
using hal::Error;
using hal::Frame;
using hal::PixelFormat;
using hal::Result;
using hal::Status;

namespace {

constexpr unsigned kBufferCount = 2;  // capture ring: enough for stills + modest streaming

/// ioctl with EINTR retry — the canonical V4L2 wrapper.
int xioctl(int fd, unsigned long request, void* arg) {
    int r;
    do {
        r = ::ioctl(fd, request, arg);
    } while (r == -1 && errno == EINTR);
    return r;
}

std::uint32_t toV4l2(PixelFormat format) {
    switch (format) {
        case PixelFormat::Gray8:
            return V4L2_PIX_FMT_GREY;
        case PixelFormat::RGB888:
            return V4L2_PIX_FMT_RGB24;
        case PixelFormat::YUYV:
            return V4L2_PIX_FMT_YUYV;
        case PixelFormat::MJPEG:
            return V4L2_PIX_FMT_MJPEG;
        case PixelFormat::NV12:
            return V4L2_PIX_FMT_NV12;
        case PixelFormat::Unknown:
            break;
    }
    return 0;
}

PixelFormat fromV4l2(std::uint32_t fourcc) {
    switch (fourcc) {
        case V4L2_PIX_FMT_GREY:
            return PixelFormat::Gray8;
        case V4L2_PIX_FMT_RGB24:
            return PixelFormat::RGB888;
        case V4L2_PIX_FMT_YUYV:
            return PixelFormat::YUYV;
        case V4L2_PIX_FMT_MJPEG:
            return PixelFormat::MJPEG;
        case V4L2_PIX_FMT_NV12:
            return PixelFormat::NV12;
        default:
            return PixelFormat::Unknown;
    }
}

}  // namespace

V4l2Camera::V4l2Camera(std::string devicePath) : devicePath_(std::move(devicePath)) {}

V4l2Camera::~V4l2Camera() {
    close();
}

std::string V4l2Camera::deviceIdentity() const {
    // Probe with a transient fd so identity works before open().
    const int fd = fd_ >= 0 ? fd_ : ::open(devicePath_.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) return {};
    v4l2_capability cap{};
    std::string identity;
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
        identity = std::string(reinterpret_cast<const char*>(cap.driver)) + ": " +
                   reinterpret_cast<const char*>(cap.card);
    }
    if (fd != fd_) ::close(fd);
    return identity;
}

std::vector<CameraMode> V4l2Camera::supportedModes() const {
    std::vector<CameraMode> modes;
    const int fd = fd_ >= 0 ? fd_ : ::open(devicePath_.c_str(), O_RDWR | O_NONBLOCK);
    if (fd < 0) return modes;

    v4l2_fmtdesc fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (fmt.index = 0; xioctl(fd, VIDIOC_ENUM_FMT, &fmt) == 0; ++fmt.index) {
        const PixelFormat format = fromV4l2(fmt.pixelformat);
        if (format == PixelFormat::Unknown) continue;

        v4l2_frmsizeenum size{};
        size.pixel_format = fmt.pixelformat;
        for (size.index = 0; xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size) == 0; ++size.index) {
            if (size.type != V4L2_FRMSIZE_TYPE_DISCRETE) break;
            CameraMode mode;
            mode.width = static_cast<std::uint16_t>(size.discrete.width);
            mode.height = static_cast<std::uint16_t>(size.discrete.height);
            mode.format = format;
            mode.fps = 0.0f;  // interval enumeration deferred until something needs it
            modes.push_back(mode);
        }
    }
    if (fd != fd_) ::close(fd);
    return modes;
}

Status V4l2Camera::open(const CameraMode& mode) {
    if (fd_ >= 0) return Error::Busy;
    const std::uint32_t fourcc = toV4l2(mode.format);
    if (fourcc == 0 || mode.width == 0 || mode.height == 0) return Error::InvalidArgument;

    fd_ = ::open(devicePath_.c_str(), O_RDWR | O_NONBLOCK);
    if (fd_ < 0) return Error::IoFailure;

    v4l2_capability cap{};
    if (xioctl(fd_, VIDIOC_QUERYCAP, &cap) == -1 || !(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING)) {
        close();
        return Error::NotSupported;
    }

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = mode.width;
    fmt.fmt.pix.height = mode.height;
    fmt.fmt.pix.pixelformat = fourcc;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (xioctl(fd_, VIDIOC_S_FMT, &fmt) == -1) {
        close();
        return Error::IoFailure;
    }
    // The driver may adjust the request; record what it actually granted.
    mode_.width = static_cast<std::uint16_t>(fmt.fmt.pix.width);
    mode_.height = static_cast<std::uint16_t>(fmt.fmt.pix.height);
    mode_.format = fromV4l2(fmt.fmt.pix.pixelformat);
    mode_.fps = mode.fps;
    if (mode_.format == PixelFormat::Unknown) {
        close();
        return Error::NotSupported;
    }

    v4l2_requestbuffers req{};
    req.count = kBufferCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_REQBUFS, &req) == -1 || req.count < 1) {
        close();
        return Error::IoFailure;
    }

    for (unsigned i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) == -1) {
            close();
            return Error::IoFailure;
        }
        MappedBuffer mapped;
        mapped.length = buf.length;
        mapped.start =
            ::mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
        if (mapped.start == MAP_FAILED) {
            close();
            return Error::IoFailure;
        }
        buffers_.push_back(mapped);
        if (xioctl(fd_, VIDIOC_QBUF, &buf) == -1) {
            close();
            return Error::IoFailure;
        }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_STREAMON, &type) == -1) {
        close();
        return Error::IoFailure;
    }
    streaming_ = true;
    return {};
}

void V4l2Camera::releaseBuffers() {
    for (auto& buffer : buffers_)
        if (buffer.start && buffer.start != MAP_FAILED) ::munmap(buffer.start, buffer.length);
    buffers_.clear();
}

Status V4l2Camera::close() {
    stopStream();
    if (fd_ >= 0) {
        if (streaming_) {
            v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            xioctl(fd_, VIDIOC_STREAMOFF, &type);
            streaming_ = false;
        }
        releaseBuffers();
        ::close(fd_);
        fd_ = -1;
    }
    return {};
}

bool V4l2Camera::isOpen() const noexcept {
    return fd_ >= 0 && streaming_;
}

Status V4l2Camera::setControls(const hal::CameraControls&) {
    // Exposure/gain/focus mapping to V4L2 CIDs is deliberately deferred until
    // Scout calibration needs it (ROADMAP camera/libcamera-controls).
    return Error::NotSupported;
}

hal::Result<Frame> V4l2Camera::dequeueFrame(std::chrono::milliseconds timeout) {
    if (!isOpen()) return Error::NotInitialized;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    timeval tv{};
    tv.tv_sec = static_cast<long>(timeout.count() / 1000);
    tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
    const int ready = ::select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (ready == 0) return Error::Timeout;
    if (ready < 0) return Error::IoFailure;

    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_DQBUF, &buf) == -1)
        return errno == EAGAIN ? Error::Timeout : Error::IoFailure;
    if (buf.index >= buffers_.size()) return Error::IoFailure;

    // Copy out so the mmap ring can be requeued immediately. bytesused is the
    // real payload size (MJPEG frames are much smaller than the buffer).
    const auto* src = static_cast<const std::byte*>(buffers_[buf.index].start);
    auto pixels = std::make_shared<std::vector<std::byte>>(src, src + buf.bytesused);

    if (xioctl(fd_, VIDIOC_QBUF, &buf) == -1) return Error::IoFailure;

    return Frame(mode_, std::move(pixels), std::chrono::steady_clock::now());
}

hal::Result<Frame> V4l2Camera::capture(std::chrono::milliseconds timeout) {
    if (streamRunning_) return Error::Busy;  // the stream thread owns the queue
    return dequeueFrame(timeout);
}

Status V4l2Camera::startStream(std::function<void(const Frame&)> onFrame) {
    if (!isOpen()) return Error::NotInitialized;
    if (streamRunning_) return Error::Busy;

    streamRunning_ = true;
    streamThread_ = std::thread([this, onFrame = std::move(onFrame)] {
        while (streamRunning_) {
            auto frame = dequeueFrame(std::chrono::milliseconds(500));
            if (frame)
                onFrame(frame.value());
            else if (frame.error() != Error::Timeout)
                break;  // device gone
        }
    });
    return {};
}

Status V4l2Camera::stopStream() {
    streamRunning_ = false;
    if (streamThread_.joinable()) streamThread_.join();
    return {};
}

}  // namespace platypus::unoq
