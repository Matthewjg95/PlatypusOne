#include "HostSimBoard.hpp"
#include "Win32SimDisplay.hpp"

#include <cstdlib>
#include <filesystem>

namespace platypus::sim {

namespace {

/// 320x240 RGB565 display that accepts frames and discards them.
/// TODO(sim): dump to PNG / SDL window for visual debugging.
class SimDisplay final : public hal::IDisplay {
public:
    hal::DisplayInfo info() const noexcept override { return {320, 240, 16}; }
    hal::Status setBacklight(float) override { return {}; }
    hal::Status present(std::span<const std::byte> pixels) override {
        const auto expected = std::size_t{320} * 240 * 2;
        return pixels.size() == expected ? hal::Status{}
                                         : hal::Status{hal::Error::InvalidArgument};
    }
    hal::Status onTouch(std::function<void(const hal::TouchEvent&)> h) override {
        touch_ = std::move(h);
        return {};
    }
    hal::Status onButton(std::function<void(const hal::ButtonEvent&)> h) override {
        button_ = std::move(h);
        return {};
    }

private:
    std::function<void(const hal::TouchEvent&)> touch_;
    std::function<void(const hal::ButtonEvent&)> button_;
};

class SimStorage final : public hal::IStorage {
public:
    SimStorage() : root_(std::filesystem::temp_directory_path() / "platypusos") {
        std::error_code ec;
        std::filesystem::create_directories(root_, ec);
    }
    std::filesystem::path dataRoot() const override { return root_; }
    hal::Result<std::uint64_t> bytesFree() const override {
        std::error_code ec;
        const auto info = std::filesystem::space(root_, ec);
        if (ec) return hal::Error::IoFailure;
        return static_cast<std::uint64_t>(info.available);
    }
    hal::Result<std::uint64_t> bytesTotal() const override {
        std::error_code ec;
        const auto info = std::filesystem::space(root_, ec);
        if (ec) return hal::Error::IoFailure;
        return static_cast<std::uint64_t>(info.capacity);
    }
    bool isRemovable() const noexcept override { return false; }

private:
    std::filesystem::path root_;
};

}  // namespace

HostSimBoard::HostSimBoard()
    :
#ifdef _WIN32
      display_(std::make_shared<Win32SimDisplay>()),
#else
      display_(std::make_shared<SimDisplay>()),
#endif
      storage_(std::make_shared<SimStorage>()) {}

}  // namespace platypus::sim
