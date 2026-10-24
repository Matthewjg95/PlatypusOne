// TofMonitorClient — reads the VL53L8CX sketch's records from arduino-router.
//
// A background thread connects to the Bridge Monitor (TCP 127.0.0.1:7500),
// asks for STATUS and START, and keeps the newest frame plus the counters a
// display needs to be honest about the stream (rejects, last event, rate).
// The Monitor serves one client well, so whoever owns the depth view owns the
// sensor: stop() disconnects, which frees it for tools/tof_unoq/tof_log.py.
// Disconnecting does not stop ranging; the sketch keeps running.
#pragma once

#include <platypus/hal/tof/TofProtocol.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace platypus::hal::tof {

class TofMonitorClient {
   public:
    explicit TofMonitorClient(std::string host = "127.0.0.1", std::uint16_t port = 7500);
    ~TofMonitorClient();
    TofMonitorClient(const TofMonitorClient&) = delete;
    TofMonitorClient& operator=(const TofMonitorClient&) = delete;

    void start();
    void stop();

    struct Snapshot {
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
    [[nodiscard]] Snapshot snapshot() const;

    [[nodiscard]] std::string endpoint() const;

   private:
    void run();
    void handle(const std::string& line);

    std::string host_;
    std::uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    mutable std::mutex mutex_;
    Snapshot state_;
};

}  // namespace platypus::hal::tof
