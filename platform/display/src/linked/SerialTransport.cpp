#include "SerialTransport.hpp"

#include <cerrno>

#include <fcntl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

namespace platypus::linked {

using hal::Error;

SerialTransport::SerialTransport(std::string devicePath) : devicePath_(std::move(devicePath)) {}

SerialTransport::~SerialTransport() {
    if (fd_ >= 0) ::close(fd_);
}

hal::Status SerialTransport::open() {
    if (fd_ >= 0) return Error::Busy;
    fd_ = ::open(devicePath_.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) return Error::IoFailure;

    termios tio{};
    if (tcgetattr(fd_, &tio) != 0) {
        ::close(fd_);
        fd_ = -1;
        return Error::IoFailure;
    }
    cfmakeraw(&tio);
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
    if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
        ::close(fd_);
        fd_ = -1;
        return Error::IoFailure;
    }
    return {};
}

hal::Status SerialTransport::write(std::span<const std::byte> data) {
    if (fd_ < 0) return Error::NotInitialized;
    std::size_t sent = 0;
    while (sent < data.size()) {
        const auto wrote = ::write(fd_, data.data() + sent, data.size() - sent);
        if (wrote < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) {  // wait for the fd to drain
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(fd_, &fds);
                timeval tv{0, 100000};  // 100 ms
                if (::select(fd_ + 1, nullptr, &fds, nullptr, &tv) <= 0) return Error::Timeout;
                continue;
            }
            return Error::IoFailure;
        }
        sent += static_cast<std::size_t>(wrote);
    }
    return {};
}

hal::Result<std::size_t> SerialTransport::read(std::span<std::byte> buffer,
                                               std::chrono::milliseconds timeout) {
    if (fd_ < 0) return Error::NotInitialized;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd_, &fds);
    timeval tv{static_cast<long>(timeout.count() / 1000),
               static_cast<long>((timeout.count() % 1000) * 1000)};
    const int ready = ::select(fd_ + 1, &fds, nullptr, nullptr, &tv);
    if (ready < 0)
        return errno == EINTR ? hal::Result<std::size_t>(std::size_t{0})
                              : hal::Result<std::size_t>(Error::IoFailure);
    if (ready == 0) return std::size_t{0};  // timeout: caller polls again

    const auto got = ::read(fd_, buffer.data(), buffer.size());
    if (got < 0)
        return errno == EAGAIN || errno == EINTR ? hal::Result<std::size_t>(std::size_t{0})
                                                 : hal::Result<std::size_t>(Error::IoFailure);
    if (got == 0) return Error::IoFailure;  // EOF: device unplugged
    return static_cast<std::size_t>(got);
}

}  // namespace platypus::linked
