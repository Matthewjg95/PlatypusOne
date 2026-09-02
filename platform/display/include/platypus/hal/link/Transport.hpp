// PlatypusOS HAL — presentation-link transport seam.
//
// The link protocol (docs/protocols/presentation.md §3) is transport-agnostic:
// any reliable, ordered byte stream qualifies. Concrete transports are USB CDC
// serial today and TCP later; tests use an in-memory fake. Framing is the
// protocol's own business — transports move bytes and nothing else.
#pragma once

#include <platypus/hal/Result.hpp>

#include <chrono>
#include <cstddef>
#include <span>

namespace platypus::hal::link {

class ITransport {
   public:
    virtual ~ITransport() = default;

    /// Writes the whole buffer or fails. Blocking; the link's frames are small
    /// enough (≤ 64 KiB) that partial-write handling lives inside transports.
    virtual Status write(std::span<const std::byte> data) = 0;

    /// Reads up to buffer.size() bytes, returning the count actually read.
    /// Returns 0 on timeout (not an error — the caller polls). Must return
    /// promptly once timeout elapses so receive threads can shut down.
    virtual Result<std::size_t> read(std::span<std::byte> buffer,
                                     std::chrono::milliseconds timeout) = 0;
};

}  // namespace platypus::hal::link
