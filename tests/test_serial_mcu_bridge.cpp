#include <unoq/SerialMcuBridge.hpp>

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

using platypus::hal::Error;
using platypus::unoq::SerialMcuBridge;

namespace {
bool rejectConfiguration = false;
int configurationCalls = 0;
}  // namespace

extern "C" int __real_tcsetattr(int fd, int action, const termios* settings);
extern "C" int __wrap_tcsetattr(int fd, int action, const termios* settings) {
    ++configurationCalls;
    if (rejectConfiguration) {
        errno = EIO;
        return -1;
    }
    return __real_tcsetattr(fd, action, settings);
}

int main() {
    SerialMcuBridge invalid("/definitely-not-a-device", 12345);
    assert(invalid.open().error() == Error::InvalidArgument);
    SerialMcuBridge zero("/definitely-not-a-device", 0);
    assert(zero.open().error() == Error::InvalidArgument);
    SerialMcuBridge missing("/definitely-not-a-device", 9600);
    assert(missing.open().error() == Error::IoFailure);
    SerialMcuBridge notTty("/dev/null");
    assert(notTty.open().error() == Error::IoFailure);
    assert(notTty.open().error() == Error::IoFailure);

    const int master = ::posix_openpt(O_RDWR | O_NOCTTY);
    assert(master >= 0);
    assert(::grantpt(master) == 0);
    assert(::unlockpt(master) == 0);
    const char* path = ::ptsname(master);
    assert(path);
    const int observer = ::open(path, O_RDWR | O_NOCTTY);
    assert(observer >= 0);

    const unsigned rates[] = {9600, 115200, 230400};
    const speed_t speeds[] = {B9600, B115200, B230400};
    for (int i = 0; i < 3; ++i) {
        SerialMcuBridge bridge(path, rates[i]);
        assert(bridge.open());
        assert(bridge.open());  // idempotent
        termios applied{};
        assert(::tcgetattr(observer, &applied) == 0);
        assert(::cfgetispeed(&applied) == speeds[i]);
        assert(::cfgetospeed(&applied) == speeds[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        const auto start = std::chrono::steady_clock::now();
        bridge.close();
        assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
        assert(bridge.open());
        bridge.close();
    }

    SerialMcuBridge retry(path, 9600);
    rejectConfiguration = true;
    assert(retry.open().error() == Error::IoFailure);
    // Failed open must not leave fd_ looking initialized.
    assert(retry.open().error() == Error::IoFailure);
    rejectConfiguration = false;
    assert(retry.open());
    std::thread peer([master]() {
        pollfd input{master, POLLIN, 0};
        assert(::poll(&input, 1, 2000) == 1);
        std::byte incoming[256];
        const auto count = ::read(master, incoming, sizeof(incoming));
        assert(count > 0);
        platypus::hal::mcu::Decoder decoder;
        bool ping = false;
        for (ssize_t i = 0; i < count; ++i) {
            const auto message = decoder.feed(incoming[i]);
            if (message) ping = message->topic == platypus::hal::mcu::topics::kPing;
        }
        assert(ping);
        const auto pong = platypus::hal::mcu::encode(platypus::hal::mcu::topics::kPong, {});
        assert(::write(master, pong.data(), pong.size()) == static_cast<ssize_t>(pong.size()));
    });
    assert(retry.ping(std::chrono::seconds(2)));
    peer.join();
    retry.close();

    // A PTY is only a transport-policy stand-in; no physical RPMsg claim.
    const int before = configurationCalls;
    rejectConfiguration = true;
    SerialMcuBridge rpmsg(path, 12345, SerialMcuBridge::Transport::Rpmsg);
    assert(rpmsg.open());
    assert(configurationCalls == before);
    rpmsg.close();
    rejectConfiguration = false;

    ::close(observer);
    ::close(master);
    std::cout << "Serial MCU bridge host checks passed\n";
}
