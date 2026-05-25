#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tcp {

enum class PixelFormat : std::uint8_t {
    U8,
    U16,
    F32,
};

constexpr std::size_t bytes_per_sample(PixelFormat fmt) noexcept
{
    switch (fmt) {
        case PixelFormat::U8: return 1;
        case PixelFormat::U16: return 2;
        case PixelFormat::F32: return 4;
    }
    return 0;
}

constexpr std::string_view to_string(PixelFormat fmt) noexcept
{
    switch (fmt) {
        case PixelFormat::U8: return "u8";
        case PixelFormat::U16: return "u16";
        case PixelFormat::F32: return "f32";
    }
    return "?";
}

} // namespace tcp
