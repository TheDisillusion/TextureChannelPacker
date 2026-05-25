#pragma once

#include "tcp/image.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace tcp {

enum class ResizeFilter : std::uint8_t {
    Nearest,
    Bilinear,
    Mitchell,
    Lanczos,
};

constexpr std::string_view to_string(ResizeFilter f) noexcept
{
    switch (f) {
        case ResizeFilter::Nearest: return "nearest";
        case ResizeFilter::Bilinear: return "bilinear";
        case ResizeFilter::Mitchell: return "mitchell";
        case ResizeFilter::Lanczos: return "lanczos3";
    }
    return "mitchell";
}

struct ResizeResult
{
    Image image;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return error.empty() && !image.empty(); }
};

// High-quality resample using OpenImageIO's ImageBufAlgo::resize. If the
// requested size matches the source, a copy is returned without resampling.
ResizeResult resize(const Image& src, int new_width, int new_height, ResizeFilter filter);

} // namespace tcp
