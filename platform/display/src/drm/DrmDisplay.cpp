#include "drm/DrmDisplay.hpp"

#include "drm/KmsHelpers.hpp"

#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <vector>

namespace platypus::drm {

using hal::Error;
using hal::Status;

struct DrmDisplay::SavedCrtc {
    drm_mode_crtc crtc{};
};

namespace {

int xioctl(int fd, unsigned long request, void* arg) {
    int result = 0;
    do {
        result = ::ioctl(fd, request, arg);
    } while (result == -1 && (errno == EINTR || errno == EAGAIN));
    return result;
}

template <typename T>
std::uint64_t userPtr(T* p) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p));
}

struct Resources {
    std::vector<std::uint32_t> crtcs;
    std::vector<std::uint32_t> connectors;
};

/// DRM_IOCTL_MODE_GETRESOURCES is two-pass (counts, then arrays), and a
/// hotplug between passes can grow the counts — retry until they are stable.
std::optional<Resources> getResources(int fd) {
    for (int attempt = 0; attempt < 4; ++attempt) {
        drm_mode_card_res counts{};
        if (xioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &counts) == -1) return std::nullopt;
        Resources r;
        r.crtcs.resize(counts.count_crtcs);
        r.connectors.resize(counts.count_connectors);
        std::vector<std::uint32_t> fbs(counts.count_fbs);
        std::vector<std::uint32_t> encoders(counts.count_encoders);
        drm_mode_card_res full{};
        full.count_crtcs = counts.count_crtcs;
        full.crtc_id_ptr = userPtr(r.crtcs.data());
        full.count_connectors = counts.count_connectors;
        full.connector_id_ptr = userPtr(r.connectors.data());
        full.count_fbs = counts.count_fbs;
        full.fb_id_ptr = userPtr(fbs.data());
        full.count_encoders = counts.count_encoders;
        full.encoder_id_ptr = userPtr(encoders.data());
        if (xioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &full) == -1) return std::nullopt;
        if (full.count_crtcs > r.crtcs.size() || full.count_connectors > r.connectors.size() ||
            full.count_fbs > fbs.size() || full.count_encoders > encoders.size())
            continue;
        r.crtcs.resize(full.count_crtcs);
        r.connectors.resize(full.count_connectors);
        return r;
    }
    return std::nullopt;
}

struct Connector {
    drm_mode_get_connector info{};
    std::vector<drm_mode_modeinfo> modes;
    std::vector<std::uint32_t> encoders;
};

std::optional<Connector> getConnector(int fd, std::uint32_t id) {
    for (int attempt = 0; attempt < 4; ++attempt) {
        drm_mode_get_connector counts{};
        counts.connector_id = id;
        if (xioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &counts) == -1) return std::nullopt;
        Connector c;
        c.modes.resize(counts.count_modes);
        c.encoders.resize(counts.count_encoders);
        c.info.connector_id = id;
        c.info.count_modes = counts.count_modes;
        c.info.modes_ptr = userPtr(c.modes.data());
        c.info.count_encoders = counts.count_encoders;
        c.info.encoders_ptr = userPtr(c.encoders.data());
        if (xioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c.info) == -1) return std::nullopt;
        if (c.info.count_modes > c.modes.size() || c.info.count_encoders > c.encoders.size())
            continue;
        c.modes.resize(c.info.count_modes);
        c.encoders.resize(c.info.count_encoders);
        return c;
    }
    return std::nullopt;
}

/// The CRTC the connector's current encoder drives, else the first CRTC any of
/// its encoders can drive.
std::uint32_t findCrtc(int fd, const Connector& connector, const Resources& resources) {
    if (connector.info.encoder_id != 0) {
        drm_mode_get_encoder encoder{};
        encoder.encoder_id = connector.info.encoder_id;
        if (xioctl(fd, DRM_IOCTL_MODE_GETENCODER, &encoder) == 0 && encoder.crtc_id != 0)
            return encoder.crtc_id;
    }
    for (const auto encoderId : connector.encoders) {
        drm_mode_get_encoder encoder{};
        encoder.encoder_id = encoderId;
        if (xioctl(fd, DRM_IOCTL_MODE_GETENCODER, &encoder) != 0) continue;
        for (std::size_t i = 0; i < resources.crtcs.size() && i < 32; ++i)
            if (encoder.possible_crtcs & (1u << i)) return resources.crtcs[i];
    }
    return 0;
}

std::string connectorLabel(const drm_mode_get_connector& c) {
    const char* state = c.connection == 1   ? "connected"
                        : c.connection == 2 ? "disconnected"
                                            : "unknown";
    return std::string(connectorTypeName(c.connector_type)) + "-" +
           std::to_string(c.connector_type_id) + " " + state;
}

// --- evdev ------------------------------------------------------------------

constexpr std::size_t kBitsPerLong = sizeof(unsigned long) * 8;
constexpr std::size_t longsFor(std::size_t bits) {
    return (bits + kBitsPerLong - 1) / kBitsPerLong;
}

bool testBit(const unsigned long* bits, unsigned bit) {
    return (bits[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1ul;
}

/// A touchscreen as evdev presents it after the driver's single-touch pointer
/// emulation: absolute X/Y plus BTN_TOUCH. Mice (relative) and keyboards fail
/// the test, which is what lets the device be found without knowing its name.
bool isTouchscreen(int fd) {
    unsigned long ev[longsFor(EV_MAX + 1)] = {};
    unsigned long abs[longsFor(ABS_MAX + 1)] = {};
    unsigned long key[longsFor(KEY_MAX + 1)] = {};
    if (::ioctl(fd, EVIOCGBIT(0, sizeof(ev)), ev) < 0) return false;
    if (!testBit(ev, EV_ABS) || !testBit(ev, EV_KEY)) return false;
    if (::ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) < 0) return false;
    if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key)), key) < 0) return false;
    return testBit(abs, ABS_X) && testBit(abs, ABS_Y) && testBit(key, BTN_TOUCH);
}

std::string evdevName(const std::filesystem::path& sysEntry) {
    std::ifstream in(sysEntry / "device" / "name");
    std::string name;
    std::getline(in, name);
    return name;
}

}  // namespace

// --- lifecycle --------------------------------------------------------------

DrmDisplay::DrmDisplay(DrmDisplayConfig config) : config_(std::move(config)) {}

DrmDisplay::~DrmDisplay() {
    close();
}

Status DrmDisplay::fail(Error error, std::string why) {
    diagnostic_ = std::move(why);
    close();
    return error;
}

Status DrmDisplay::open() {
    if (fd_ >= 0) return Error::Busy;
    diagnostic_.clear();
    selection_ = {};

    fd_ = ::open(config_.devicePath.c_str(), O_RDWR | O_CLOEXEC);
    if (fd_ < 0)
        return fail(Error::IoFailure, "open " + config_.devicePath + ": " + std::strerror(errno));

    if (const auto status = programDisplay(); !status) return status;
    startInput();
    return {};
}

Status DrmDisplay::programDisplay() {
    const auto resources = getResources(fd_);
    if (!resources) return fail(Error::IoFailure, "DRM_IOCTL_MODE_GETRESOURCES failed");

    std::vector<Connector> connectors;
    std::vector<ConnectorCandidate> candidates;
    std::string seen;
    for (const auto id : resources->connectors) {
        auto c = getConnector(fd_, id);
        if (!c) continue;
        seen += (seen.empty() ? "" : ", ") + connectorLabel(c->info);
        candidates.push_back(
            {id, c->info.connector_type, c->info.connection == 1, c->modes.size()});
        connectors.push_back(std::move(*c));
    }
    const auto chosen = chooseConnector(candidates, config_.preferredConnectorType);
    if (!chosen)
        return fail(
            Error::NotSupported,
            "no connected display (" + (seen.empty() ? std::string("no connectors") : seen) + ")");
    const Connector& connector = connectors[*chosen];

    std::vector<ModeCandidate> modeCandidates;
    for (const auto& m : connector.modes)
        modeCandidates.push_back(
            {m.hdisplay, m.vdisplay, m.vrefresh, (m.type & DRM_MODE_TYPE_PREFERRED) != 0});
    const auto modeIndex = chooseMode(modeCandidates);
    if (!modeIndex)
        return fail(Error::NotSupported, connectorLabel(connector.info) + " has no usable mode");
    drm_mode_modeinfo mode = connector.modes[*modeIndex];

    const auto crtcId = findCrtc(fd_, connector, *resources);
    if (crtcId == 0)
        return fail(Error::NotSupported, "no CRTC can drive " + connectorLabel(connector.info));

    auto saved = std::make_unique<SavedCrtc>();
    saved->crtc.crtc_id = crtcId;
    if (xioctl(fd_, DRM_IOCTL_MODE_GETCRTC, &saved->crtc) == 0) savedCrtc_ = std::move(saved);

    // Scanout buffer: XRGB8888, cleared to black.
    drm_mode_create_dumb create{};
    create.width = mode.hdisplay;
    create.height = mode.vdisplay;
    create.bpp = 32;
    if (xioctl(fd_, DRM_IOCTL_MODE_CREATE_DUMB, &create) == -1)
        return fail(Error::IoFailure, std::string("CREATE_DUMB: ") + std::strerror(errno));
    dumbHandle_ = create.handle;
    pitch_ = create.pitch;
    mapSize_ = static_cast<std::size_t>(create.size);

    drm_mode_fb_cmd fb{};
    fb.width = mode.hdisplay;
    fb.height = mode.vdisplay;
    fb.pitch = pitch_;
    fb.bpp = 32;
    fb.depth = 24;
    fb.handle = dumbHandle_;
    if (xioctl(fd_, DRM_IOCTL_MODE_ADDFB, &fb) == -1)
        return fail(Error::IoFailure, std::string("ADDFB: ") + std::strerror(errno));
    fbId_ = fb.fb_id;

    drm_mode_map_dumb mapReq{};
    mapReq.handle = dumbHandle_;
    if (xioctl(fd_, DRM_IOCTL_MODE_MAP_DUMB, &mapReq) == -1)
        return fail(Error::IoFailure, std::string("MAP_DUMB: ") + std::strerror(errno));
    void* mapped = ::mmap(nullptr, mapSize_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_,
                          static_cast<off_t>(mapReq.offset));
    if (mapped == MAP_FAILED)
        return fail(Error::IoFailure, std::string("mmap: ") + std::strerror(errno));
    map_ = static_cast<std::byte*>(mapped);
    std::memset(map_, 0, mapSize_);

    std::uint32_t connectorId = connector.info.connector_id;
    drm_mode_crtc set{};
    set.crtc_id = crtcId;
    set.fb_id = fbId_;
    set.set_connectors_ptr = userPtr(&connectorId);
    set.count_connectors = 1;
    set.mode = mode;
    set.mode_valid = 1;
    if (xioctl(fd_, DRM_IOCTL_MODE_SETCRTC, &set) == -1) {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            savedCrtc_.reset();  // nothing of ours was programmed; do not "restore"
            return fail(Error::Busy,
                        "another client holds DRM master - a display server such as lightdm/Xorg. "
                        "Stop it first (sudo systemctl stop lightdm).");
        }
        return fail(Error::IoFailure, std::string("SETCRTC: ") + std::strerror(err));
    }

    selection_.connectorId = connector.info.connector_id;
    selection_.connectorType = connector.info.connector_type;
    selection_.crtcId = crtcId;
    selection_.width = mode.hdisplay;
    selection_.height = mode.vdisplay;
    selection_.refreshHz = mode.vrefresh;
    return {};
}

void DrmDisplay::releaseScanout() {
    if (map_) {
        ::munmap(map_, mapSize_);
        map_ = nullptr;
    }
    if (fbId_ != 0) {
        xioctl(fd_, DRM_IOCTL_MODE_RMFB, &fbId_);
        fbId_ = 0;
    }
    if (dumbHandle_ != 0) {
        drm_mode_destroy_dumb destroy{};
        destroy.handle = dumbHandle_;
        xioctl(fd_, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        dumbHandle_ = 0;
    }
}

void DrmDisplay::close() {
    running_ = false;
    if (wakeFd_ >= 0) {
        const std::uint64_t one = 1;
        [[maybe_unused]] const auto written = ::write(wakeFd_, &one, sizeof(one));
    }
    if (inputThread_.joinable()) inputThread_.join();
    for (int* fd : {&touchFd_, &buttonFd_, &wakeFd_})
        if (*fd >= 0) {
            ::close(*fd);
            *fd = -1;
        }

    if (fd_ < 0) return;
    // Hand the CRTC back to whatever it showed before (console, or nothing).
    if (savedCrtc_ && savedCrtc_->crtc.mode_valid && selection_.connectorId != 0) {
        std::uint32_t connectorId = selection_.connectorId;
        drm_mode_crtc restore = savedCrtc_->crtc;
        restore.set_connectors_ptr = userPtr(&connectorId);
        restore.count_connectors = 1;
        xioctl(fd_, DRM_IOCTL_MODE_SETCRTC, &restore);
    }
    savedCrtc_.reset();
    releaseScanout();
    ::close(fd_);
    fd_ = -1;
}

// --- IDisplay ---------------------------------------------------------------

hal::DisplayInfo DrmDisplay::info() const noexcept {
    // The renderer draws RGB565; conversion to the scanout format is ours.
    return {selection_.width, selection_.height, 16};
}

Status DrmDisplay::present(std::span<const std::byte> pixels) {
    return presentRegion(pixels, {0, 0, selection_.width, selection_.height});
}

Status DrmDisplay::presentRegion(std::span<const std::byte> pixels,
                                 const hal::DisplayRegion& region) {
    if (!map_) return Error::NotInitialized;
    const std::size_t expected = std::size_t{selection_.width} * selection_.height * 2;
    if (pixels.size() != expected) return Error::InvalidArgument;

    const auto clipped = clampRegion(region, selection_.width, selection_.height);
    if (clipped.width == 0 || clipped.height == 0) return {};
    blitRgb565ToXrgb8888(pixels, selection_.width, selection_.height, map_, pitch_, clipped);

    // Video-mode panels and monitors scan the buffer continuously and need no
    // flush; command-mode DSI panels only update on DIRTYFB. Tell the driver
    // either way and ignore "not supported".
    drm_clip_rect clip{};
    clip.x1 = clipped.x;
    clip.y1 = clipped.y;
    clip.x2 = static_cast<unsigned short>(clipped.x + clipped.width);
    clip.y2 = static_cast<unsigned short>(clipped.y + clipped.height);
    drm_mode_fb_dirty_cmd dirty{};
    dirty.fb_id = fbId_;
    dirty.num_clips = 1;
    dirty.clips_ptr = userPtr(&clip);
    xioctl(fd_, DRM_IOCTL_MODE_DIRTYFB, &dirty);
    return {};
}

Status DrmDisplay::setBacklight(float brightness) {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/sys/class/backlight", ec)) {
        int max = 0;
        std::ifstream in(entry.path() / "max_brightness");
        if (!(in >> max) || max <= 0) continue;
        const float clamped = std::fmin(1.0f, std::fmax(0.0f, brightness));
        const auto level = static_cast<int>(std::lround(clamped * static_cast<float>(max)));
        std::ofstream out(entry.path() / "brightness");
        if (!(out << level << '\n')) {
            diagnostic_ = "backlight " + entry.path().string() + " is not writable";
            return Error::IoFailure;
        }
        return {};
    }
    diagnostic_ = "no backlight device under /sys/class/backlight";
    return Error::NotSupported;
}

Status DrmDisplay::onTouch(std::function<void(const hal::TouchEvent&)> handler) {
    std::lock_guard lock(handlerMutex_);
    touchHandler_ = std::move(handler);
    // Before open() the handler is simply held; after it, say whether any
    // touchscreen was found so a caller can fall back to buttons.
    return touchFd_ >= 0 || fd_ < 0 ? Status{} : Status{Error::NotSupported};
}

Status DrmDisplay::onButton(std::function<void(const hal::ButtonEvent&)> handler) {
    std::lock_guard lock(handlerMutex_);
    buttonHandler_ = std::move(handler);
    return {};
}

// --- input ------------------------------------------------------------------

void DrmDisplay::startInput() {
    namespace fs = std::filesystem;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator("/sys/class/input", ec)) {
        const auto base = entry.path().filename().string();
        if (base.rfind("event", 0) != 0) continue;
        const std::string node = "/dev/input/" + base;
        const auto name = evdevName(entry.path());

        if (buttonFd_ < 0 && !config_.buttonDeviceName.empty() &&
            name == config_.buttonDeviceName) {
            buttonFd_ = ::open(node.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
            if (buttonFd_ >= 0) selection_.buttonDevice = node + " (" + name + ")";
            continue;
        }
        if (touchFd_ < 0 && config_.enableTouch) {
            const int fd = ::open(node.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
            if (fd < 0) continue;
            input_absinfo ax{};
            input_absinfo ay{};
            if (isTouchscreen(fd) && ::ioctl(fd, EVIOCGABS(ABS_X), &ax) == 0 &&
                ::ioctl(fd, EVIOCGABS(ABS_Y), &ay) == 0) {
                touchFd_ = fd;
                touchMinX_ = ax.minimum;
                touchMaxX_ = ax.maximum;
                touchMinY_ = ay.minimum;
                touchMaxY_ = ay.maximum;
                selection_.touchDevice = node + " (" + name + ")";
            } else {
                ::close(fd);
            }
        }
    }
    if (touchFd_ < 0 && buttonFd_ < 0) return;

    wakeFd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wakeFd_ < 0) return;
    running_ = true;
    inputThread_ = std::thread([this] { inputLoop(); });
}

void DrmDisplay::inputLoop() {
    std::int32_t rawX = 0, rawY = 0;
    bool touching = false, wasTouching = false;
    std::uint16_t lastX = 0, lastY = 0;

    std::vector<pollfd> fds;
    fds.push_back({wakeFd_, POLLIN, 0});
    if (touchFd_ >= 0) fds.push_back({touchFd_, POLLIN, 0});
    if (buttonFd_ >= 0) fds.push_back({buttonFd_, POLLIN, 0});

    input_event events[64];
    while (running_) {
        if (::poll(fds.data(), static_cast<nfds_t>(fds.size()), -1) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (fds[0].revents & POLLIN) break;

        for (std::size_t i = 1; i < fds.size(); ++i) {
            if (!(fds[i].revents & POLLIN)) continue;
            const auto got = ::read(fds[i].fd, events, sizeof(events));
            if (got <= 0) continue;
            const auto count = static_cast<std::size_t>(got) / sizeof(input_event);

            for (std::size_t k = 0; k < count; ++k) {
                const auto& e = events[k];
                if (fds[i].fd == buttonFd_) {
                    if (e.type != EV_KEY || e.value == 2) continue;  // skip auto-repeat
                    std::uint8_t id = 0;
                    if (e.code == KEY_VOLUMEUP)
                        id = kBoardButtonA;
                    else if (e.code == KEY_VOLUMEDOWN)
                        id = kBoardButtonB;
                    else
                        continue;
                    std::function<void(const hal::ButtonEvent&)> handler;
                    {
                        std::lock_guard lock(handlerMutex_);
                        handler = buttonHandler_;
                    }
                    if (handler) handler({id, e.value == 1});
                    continue;
                }
                // Touchscreen: accumulate until SYN_REPORT, then emit one event.
                if (e.type == EV_ABS && e.code == ABS_X)
                    rawX = e.value;
                else if (e.type == EV_ABS && e.code == ABS_Y)
                    rawY = e.value;
                else if (e.type == EV_KEY && e.code == BTN_TOUCH)
                    touching = e.value != 0;
                else if (e.type == EV_SYN && e.code == SYN_REPORT) {
                    const auto x = scaleAxis(rawX, touchMinX_, touchMaxX_, selection_.width);
                    const auto y = scaleAxis(rawY, touchMinY_, touchMaxY_, selection_.height);
                    std::optional<hal::TouchEvent> out;
                    if (touching && !wasTouching)
                        out = hal::TouchEvent{hal::TouchEvent::Type::Down, x, y};
                    else if (touching && (x != lastX || y != lastY))
                        out = hal::TouchEvent{hal::TouchEvent::Type::Move, x, y};
                    else if (!touching && wasTouching)
                        out = hal::TouchEvent{hal::TouchEvent::Type::Up, lastX, lastY};
                    wasTouching = touching;
                    if (touching) {
                        lastX = x;
                        lastY = y;
                    }
                    if (out) {
                        std::function<void(const hal::TouchEvent&)> handler;
                        {
                            std::lock_guard lock(handlerMutex_);
                            handler = touchHandler_;
                        }
                        if (handler) handler(*out);
                    }
                }
            }
        }
    }
}

}  // namespace platypus::drm
