// PlatypusOS — IDisplay over the presentation link (the ADR-0001 prototype
// display path; protocol in docs/protocols/presentation.md).
//
// The external client (M5Stack Tab5 first) is an IDisplay implementation and
// nothing more: it receives pixel tiles and returns input events. No app or
// service learns the panel is remote; a future integrated panel replaces this
// class and nothing above the HAL changes.
//
// Threading (spec §9): a receive thread owns the transport's read side and
// invokes the registered touch/button handlers directly — the composition
// root's handlers post into appfw::EventQueue, so apps still run on the UI
// thread. present()/presentRegion() run on the caller's thread and block for
// the frame ack (bounded).
//
// Portable: depends only on ITransport; the codec is the shared Framing.hpp.
// Host tests drive it through an in-memory fake transport, no hardware.
#pragma once

#include <platypus/hal/IDisplay.hpp>
#include <platypus/hal/link/Framing.hpp>
#include <platypus/hal/link/Transport.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace platypus::linked {

struct LinkedDisplayConfig {
    std::string hostName = "platypus-one";
    std::chrono::milliseconds helloTimeout{1000};
    std::chrono::milliseconds ackTimeout{200};
};

/// Synthetic button ids for encoder detents until the HAL gains an encoder
/// event (spec §5 "Input"): one press event per detent, no release pair.
inline constexpr std::uint8_t kEncoderClockwiseButton = 254;
inline constexpr std::uint8_t kEncoderCounterClockwiseButton = 255;

class LinkedDisplay final : public hal::IDisplay {
   public:
    explicit LinkedDisplay(std::unique_ptr<hal::link::ITransport> transport,
                           LinkedDisplayConfig config = {});
    ~LinkedDisplay() override;

    LinkedDisplay(const LinkedDisplay&) = delete;
    LinkedDisplay& operator=(const LinkedDisplay&) = delete;

    /// Hello/HelloReply exchange; starts the receive thread on success. Until
    /// this succeeds the display is unusable and the board should expose it
    /// as absent.
    hal::Status connect();
    [[nodiscard]] bool connected() const noexcept { return running_.load(); }

    // hal::IDisplay
    [[nodiscard]] hal::DisplayInfo info() const noexcept override;
    hal::Status setBacklight(float brightness) override;
    hal::Status present(std::span<const std::byte> pixels) override;
    hal::Status presentRegion(std::span<const std::byte> pixels,
                              const hal::DisplayRegion& region) override;
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)> handler) override;
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)> handler) override;

   private:
    hal::Status sendFrame(std::uint16_t topic, std::span<const std::byte> payload);
    hal::Status awaitAck(std::uint16_t frameId);
    void receiveLoop();
    void handleMessage(const hal::link::Message& message);

    std::unique_ptr<hal::link::ITransport> transport_;
    LinkedDisplayConfig config_;

    hal::DisplayInfo info_{};        ///< valid only after connect()
    std::uint8_t capabilities_ = 0;  ///< HelloReply capability bits
    std::uint32_t maxPayload_ = 0;

    std::mutex writeMutex_;  ///< serializes transport writes
    std::uint16_t nextFrameId_ = 1;

    std::mutex ackMutex_;
    std::condition_variable ackCv_;
    std::optional<std::pair<std::uint16_t, std::uint8_t>> lastAck_;

    std::mutex handlerMutex_;
    std::function<void(const hal::TouchEvent&)> touchHandler_;
    std::function<void(const hal::ButtonEvent&)> buttonHandler_;

    std::atomic<bool> running_{false};
    std::thread receiveThread_;
};

}  // namespace platypus::linked
