#include "TofMonitorClient.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace platypus::hal::tof {

TofMonitorClient::TofMonitorClient(std::string host, std::uint16_t port)
    : host_(std::move(host)), port_(port) {}

TofMonitorClient::~TofMonitorClient() {
    stop();
}

std::string TofMonitorClient::endpoint() const {
    return host_ + ":" + std::to_string(port_);
}

void TofMonitorClient::start() {
    if (running_.exchange(true)) return;
    {
        std::lock_guard lock(mutex_);
        state_ = {};
    }
    thread_ = std::thread([this] { run(); });
}

void TofMonitorClient::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) thread_.join();
    std::lock_guard lock(mutex_);
    state_.connected = false;
}

TofMonitorClient::Snapshot TofMonitorClient::snapshot() const {
    std::lock_guard lock(mutex_);
    return state_;
}

void TofMonitorClient::handle(const std::string& line) {
    const auto rec = parseLine(line);
    std::lock_guard lock(mutex_);
    if (!rec) {
        ++state_.rejects;
        return;
    }
    if (rec->type == RecordType::Frame) {
        if (state_.frame && rec->frame->mcuUsReady > state_.frame->mcuUsReady) {
            const double dt =
                static_cast<double>(rec->frame->mcuUsReady - state_.frame->mcuUsReady);
            const double hz = 1e6 / dt;
            state_.hz = state_.hz > 0 ? 0.8 * state_.hz + 0.2 * hz : hz;
        }
        state_.frame = std::move(rec->frame);
        state_.frameArrived = std::chrono::steady_clock::now();
        ++state_.frames;
    } else if (rec->type == RecordType::Imu) {
        state_.imu = rec->imu;
        state_.imuArrived = std::chrono::steady_clock::now();
    } else if (rec->type == RecordType::Event && rec->code == "TRIGGER") {
        const auto edge = fieldValue(*rec, "edge");
        if (edge == "press") {
            ++state_.triggerPresses;
            state_.triggerDown = true;
        } else if (edge == "release") {
            state_.triggerDown = false;
        }
    } else if (rec->type == RecordType::Event) {
        state_.lastLevel = rec->level;
        state_.lastEvent = rec->code;
        if (rec->level == 'E') state_.lastError = rec->code;
    }
}

void TofMonitorClient::run() {
    while (running_) {
        const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        if (fd < 0 || ::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) != 1 ||
            ::connect(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
            const std::string why = std::strerror(errno);
            if (fd >= 0) ::close(fd);
            {
                std::lock_guard lock(mutex_);
                state_.connected = false;
                state_.lastError = "connect " + endpoint() + ": " + why;
            }
            for (int i = 0; i < 20 && running_; ++i)  // retry every 2 s
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        {
            std::lock_guard lock(mutex_);
            state_.connected = true;
            state_.lastError.clear();
        }
        // START brings an idle or failed sensor up; it is ignored while
        // ranging. A fresh sketch starts bring-up on the connection itself.
        const char hello[] = "STATUS\nSTART\n";
        (void)::send(fd, hello, sizeof(hello) - 1, MSG_NOSIGNAL);

        LineAssembler assembler;
        char buffer[4096];
        while (running_) {
            pollfd p{fd, POLLIN, 0};
            const int ready = ::poll(&p, 1, 200);
            if (ready < 0 && errno != EINTR) break;
            if (ready <= 0) continue;
            const auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if (n <= 0) break;  // router went away: reconnect
            for (const auto& line : assembler.feed({buffer, static_cast<std::size_t>(n)}))
                handle(line);
        }
        ::close(fd);
        std::lock_guard lock(mutex_);
        state_.connected = false;
    }
}

}  // namespace platypus::hal::tof
