// PlatypusOS HAL — persistent storage abstraction.
//
// Thin capability wrapper over the board's data partition / SD card. Higher
// level file management (projects, exports) lives in services/filesystem;
// this interface only answers "where may I write and how much space is left".
#pragma once

#include <platypus/hal/Result.hpp>

#include <cstdint>
#include <filesystem>

namespace platypus::hal {

class IStorage {
public:
    virtual ~IStorage() = default;

    /// Root directory for user data (scans, exports, settings).
    [[nodiscard]] virtual std::filesystem::path dataRoot() const = 0;

    [[nodiscard]] virtual Result<std::uint64_t> bytesFree() const = 0;
    [[nodiscard]] virtual Result<std::uint64_t> bytesTotal() const = 0;

    /// True when backed by removable media that may vanish at runtime.
    [[nodiscard]] virtual bool isRemovable() const noexcept = 0;
};

}  // namespace platypus::hal
