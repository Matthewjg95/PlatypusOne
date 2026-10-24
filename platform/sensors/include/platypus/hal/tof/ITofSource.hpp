// PlatypusOS HAL — a stream of ToF frames, as an applet sees it.
//
// The device implements it with TofMonitorClient (the UNO Q MCU sketch over
// arduino-router's Monitor); hosts and tests use FakeTofSource. start()/stop()
// bracket ownership: the Monitor serves one client well, so an applet holds
// the stream only while it is open.
#pragma once

#include <platypus/hal/tof/TofProtocol.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace platypus::hal::tof {

struct TofSnapshot {
    bool connected = false;
    std::optional<Frame> frame;
    std::chrono::steady_clock::time_point frameArrived{};
    double hz = 0.0;  ///< from MCU frame-ready times
    std::uint64_t frames = 0;
    std::uint64_t rejects = 0;  ///< CRC/format failures since start()
    char lastLevel = 0;         ///< level of the newest EVT
    std::string lastEvent;      ///< code of the newest EVT
    std::string lastError;      ///< newest EVT at level E, or the connect error
};

class ITofSource {
   public:
    virtual ~ITofSource() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual TofSnapshot snapshot() const = 0;
    [[nodiscard]] virtual std::string endpoint() const = 0;
};

/// A connected source serving one fixed frame: a tilted wall at ~214..233 mm
/// with a block at ~150 mm and an invalid corner, as recorded on the bench.
class FakeTofSource final : public ITofSource {
   public:
    FakeTofSource();
    void start() override { running_ = true; }
    void stop() override { running_ = false; }
    [[nodiscard]] TofSnapshot snapshot() const override;
    [[nodiscard]] std::string endpoint() const override { return "fake"; }
    [[nodiscard]] bool running() const noexcept { return running_; }

   private:
    Frame frame_;
    bool running_ = false;
};

}  // namespace platypus::hal::tof
