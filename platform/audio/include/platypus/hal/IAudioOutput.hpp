// PlatypusOS HAL — audio output (UI feedback tones, alerts).
#pragma once

#include <platypus/hal/Result.hpp>

#include <chrono>
#include <cstdint>
#include <span>

namespace platypus::hal {

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;

    virtual Status setVolume(float volume) = 0;  ///< 0..1

    /// Simple tone for UI feedback; non-blocking.
    virtual Status beep(float frequencyHz, std::chrono::milliseconds duration) = 0;

    /// PCM playback (mono, 16-bit signed, given sample rate); non-blocking.
    virtual Status play(std::span<const std::int16_t> samples, std::uint32_t sampleRate) = 0;
    virtual Status stop() = 0;
};

}  // namespace platypus::hal
