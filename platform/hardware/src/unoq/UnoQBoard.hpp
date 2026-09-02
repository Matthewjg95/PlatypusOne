// PlatypusOS — Arduino UNO Q board implementation (Linux MPU side).
//
// M1 bring-up status:
//   mcuBridge  ✔ SerialMcuBridge over /dev/ttyRPMSG0 (override via ctor)
//   storage    ✔ fixed data partition path
//   display    ✖ TODO(board-bringup): panel driver pending hardware selection
//   camera     ✖ TODO(board-bringup): libcamera backend (ROADMAP camera/libcamera)
//   sensors    ✖ TODO(board-bringup): IMU via MCU bridge (ROADMAP sensors/imu)
//   audio      ✖ TODO(board-bringup)
// Missing capabilities return nullptr per the IBoard contract; apps degrade.
#pragma once

#include <platypus/hal/IBoard.hpp>
#include <platypus/hal/IStorage.hpp>

#include "SerialMcuBridge.hpp"

namespace platypus::unoq {

class UnoQBoard final : public hal::IBoard {
   public:
    struct Config {
        std::string mcuDevice = "/dev/ttyRPMSG0";
        std::filesystem::path dataRoot = "/var/lib/platypusos";
        /// Injected by the composition root — a connected LinkedDisplay during
        /// the ADR-0001 prototype phase, a panel driver later. The board does
        /// not construct displays; it only exposes what it was given.
        std::shared_ptr<hal::IDisplay> display;
    };

    explicit UnoQBoard(Config config = {});

    [[nodiscard]] std::string_view id() const noexcept override { return "arduino-uno-q-r1"; }

    std::shared_ptr<hal::IDisplay> display() override { return config_.display; }
    std::shared_ptr<hal::ICamera> camera() override { return nullptr; }
    std::shared_ptr<hal::ISensorHub> sensors() override { return nullptr; }
    std::shared_ptr<hal::IAudioOutput> audio() override { return nullptr; }
    std::shared_ptr<hal::IStorage> storage() override { return storage_; }
    std::shared_ptr<hal::IMcuBridge> mcuBridge() override;

   private:
    Config config_;
    std::shared_ptr<hal::IStorage> storage_;
    std::shared_ptr<SerialMcuBridge> bridge_;  ///< lazily opened on first access
};

}  // namespace platypus::unoq
