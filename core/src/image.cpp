#include "tcp/image.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace tcp {

namespace {

// memcpy-based load/store to avoid alignment UB on a std::byte* (alignment 1).
template <typename T>
T load_at(const std::byte* base, std::size_t offset) noexcept
{
    T value;
    std::memcpy(&value, base + offset * sizeof(T), sizeof(T));
    return value;
}

template <typename T>
void store_at(std::byte* base, std::size_t offset, T value) noexcept
{
    std::memcpy(base + offset * sizeof(T), &value, sizeof(T));
}

constexpr int clamp_int(int v, int lo, int hi) noexcept
{
    return v < lo ? lo : (v > hi ? hi : v);
}

constexpr float clamp_unit(float v) noexcept
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

} // namespace

Image::Image(int width, int height, int channels, PixelFormat format)
    : width_(width < 0 ? 0 : width),
      height_(height < 0 ? 0 : height),
      channels_(channels < 1 ? 1 : (channels > 4 ? 4 : channels)),
      format_(format),
      data_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_)
            * static_cast<std::size_t>(channels_) * bytes_per_sample(format_))
{
}

std::size_t Image::pixel_count() const noexcept
{
    return static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
}

std::size_t Image::sample_count() const noexcept
{
    return pixel_count() * static_cast<std::size_t>(channels_);
}

std::size_t Image::row_stride_bytes() const noexcept
{
    return static_cast<std::size_t>(width_) * static_cast<std::size_t>(channels_)
           * bytes_per_sample(format_);
}

std::size_t Image::sample_offset(int x, int y, int channel) const noexcept
{
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
            + static_cast<std::size_t>(x))
               * static_cast<std::size_t>(channels_)
           + static_cast<std::size_t>(channel);
}

float Image::sample_linear(int x, int y, int channel) const noexcept
{
    if (empty()) {
        return 0.0f;
    }
    const int cx = clamp_int(x, 0, width_ - 1);
    const int cy = clamp_int(y, 0, height_ - 1);
    const int cc = clamp_int(channel, 0, channels_ - 1);
    const std::size_t off = sample_offset(cx, cy, cc);
    switch (format_) {
        case PixelFormat::U8: {
            const auto v = load_at<std::uint8_t>(data_.data(), off);
            return static_cast<float>(v) / 255.0f;
        }
        case PixelFormat::U16: {
            const auto v = load_at<std::uint16_t>(data_.data(), off);
            return static_cast<float>(v) / 65535.0f;
        }
        case PixelFormat::F32: {
            return load_at<float>(data_.data(), off);
        }
    }
    return 0.0f;
}

void Image::set_linear(int x, int y, int channel, float value) noexcept
{
    if (empty()) {
        return;
    }
    if (x < 0 || x >= width_ || y < 0 || y >= height_ || channel < 0 || channel >= channels_) {
        return;
    }
    const std::size_t off = sample_offset(x, y, channel);
    switch (format_) {
        case PixelFormat::U8: {
            const float c = clamp_unit(value);
            const auto v = static_cast<std::uint8_t>(std::lround(c * 255.0f));
            store_at(data_.data(), off, v);
            break;
        }
        case PixelFormat::U16: {
            const float c = clamp_unit(value);
            const auto v = static_cast<std::uint16_t>(std::lround(c * 65535.0f));
            store_at(data_.data(), off, v);
            break;
        }
        case PixelFormat::F32: {
            store_at(data_.data(), off, value);
            break;
        }
    }
}

void Image::clear() noexcept
{
    width_ = 0;
    height_ = 0;
    channels_ = 0;
    data_.clear();
}

} // namespace tcp
