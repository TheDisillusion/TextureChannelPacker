#pragma once

#include "tcp/pixel_format.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tcp {

// Owning, contiguous, row-major pixel buffer.
//
// The buffer layout is [row 0][row 1]...[row h-1] with each row containing
// `channels` samples per pixel and `width * channels` samples per row.
// Float samples are not clamped; integer samples are interpreted as their
// natural numeric range (0..255 for U8, 0..65535 for U16). The class makes
// no claims about colorspace — that is the caller's responsibility.
class Image
{
public:
    Image() noexcept = default;
    Image(int width, int height, int channels, PixelFormat format);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] int channels() const noexcept { return channels_; }
    [[nodiscard]] PixelFormat format() const noexcept { return format_; }
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }

    [[nodiscard]] std::size_t pixel_count() const noexcept;
    [[nodiscard]] std::size_t sample_count() const noexcept;
    [[nodiscard]] std::size_t byte_size() const noexcept { return data_.size(); }
    [[nodiscard]] std::size_t row_stride_bytes() const noexcept;

    [[nodiscard]] std::byte* data() noexcept { return data_.data(); }
    [[nodiscard]] const std::byte* data() const noexcept { return data_.data(); }

    // Read one sample as float. Integer formats are mapped to [0,1].
    // Out-of-range coordinates or channel indices are clamped.
    [[nodiscard]] float sample_linear(int x, int y, int channel) const noexcept;

    // Write one sample from a float. For integer formats the value is clamped to [0,1]
    // and then quantized. For F32 the value is stored verbatim (no clamp).
    void set_linear(int x, int y, int channel, float value) noexcept;

    // Reset to an empty image.
    void clear() noexcept;

private:
    [[nodiscard]] std::size_t sample_offset(int x, int y, int channel) const noexcept;

    int width_ = 0;
    int height_ = 0;
    int channels_ = 0;
    PixelFormat format_ = PixelFormat::U8;
    std::vector<std::byte> data_;
};

} // namespace tcp
