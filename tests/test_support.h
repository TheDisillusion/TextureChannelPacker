#pragma once

#include "tcp/image.h"
#include "tcp/pixel_format.h"

#include <array>
#include <cstdint>
#include <filesystem>

namespace tcp::test {

// Create a solid-color RGBA image at the requested format.
inline Image make_solid(int w, int h, PixelFormat fmt, float r, float g, float b, float a)
{
    Image img(w, h, 4, fmt);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            img.set_linear(x, y, 0, r);
            img.set_linear(x, y, 1, g);
            img.set_linear(x, y, 2, b);
            img.set_linear(x, y, 3, a);
        }
    }
    return img;
}

// Create a single-channel image whose value varies linearly across X (0..1).
inline Image make_horizontal_ramp(int w, int h, PixelFormat fmt)
{
    Image img(w, h, 1, fmt);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float t = (w > 1) ? static_cast<float>(x) / static_cast<float>(w - 1) : 0.0f;
            img.set_linear(x, y, 0, t);
        }
    }
    return img;
}

// Each sample in the U8 source quantizes to a step of 1/255, so a sample that
// round-trips through encode/decode should match the original within this slack.
inline constexpr float u8_round_trip_tolerance = 1.5f / 255.0f;

// Build a path inside a unique per-test temp directory under the system temp.
inline std::filesystem::path scratch_path(std::string_view name)
{
    namespace fs = std::filesystem;
    static const fs::path root = [] {
        fs::path p = fs::temp_directory_path() / "tcp_tests";
        fs::create_directories(p);
        return p;
    }();
    return root / name;
}

} // namespace tcp::test
