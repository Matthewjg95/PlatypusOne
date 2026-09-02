#include "UnoQBoard.hpp"

namespace platypus::unoq {

namespace {

class PartitionStorage final : public hal::IStorage {
   public:
    explicit PartitionStorage(std::filesystem::path root) : root_(std::move(root)) {
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

UnoQBoard::UnoQBoard() : UnoQBoard(Config{}) {}

UnoQBoard::UnoQBoard(Config config)
    : config_(std::move(config)), storage_(std::make_shared<PartitionStorage>(config_.dataRoot)) {}

std::shared_ptr<hal::IMcuBridge> UnoQBoard::mcuBridge() {
    if (!bridge_) {
        auto bridge = std::make_shared<SerialMcuBridge>(config_.mcuDevice);
        if (!bridge->open()) return nullptr;  // MCU firmware absent: degrade
        bridge_ = std::move(bridge);
    }
    return bridge_;
}

}  // namespace platypus::unoq
