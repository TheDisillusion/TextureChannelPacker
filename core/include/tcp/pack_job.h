#pragma once

#include "tcp/channel_ref.h"
#include "tcp/image.h"
#include "tcp/pixel_format.h"
#include "tcp/resize.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace tcp {

inline constexpr int slot_count = 4;
inline constexpr int output_channel_count = 4;

enum class ResizeMode : std::uint8_t {
    Largest,
    Smallest,
    FirstPopulated,
    Custom,
};

struct InputSlot
{
    std::filesystem::path path;
    std::shared_ptr<const Image> image;
    bool treat_as_srgb = false;

    [[nodiscard]] bool populated() const noexcept { return image != nullptr; }
};

struct ImageSize
{
    int width = 0;
    int height = 0;

    [[nodiscard]] bool valid() const noexcept { return width > 0 && height > 0; }
};

struct PackJob
{
    // Named "inputs" rather than "slots" because Qt's qobjectdefs.h #defines
    // `slots` to nothing, which would silently corrupt this declaration in any
    // translation unit that has already pulled in QObject machinery.
    std::array<InputSlot, slot_count> inputs{};

    // channel_map[i] describes what feeds destination channel i (R=0, G=1, B=2, A=3).
    // Default values for unset destination channels: 0 for RGB, 1 for A.
    std::array<ChannelRef, output_channel_count> channel_map{};

    ResizeMode resize_mode = ResizeMode::Largest;
    ResizeFilter resize_filter = ResizeFilter::Mitchell;
    ImageSize custom_size{1024, 1024};
    PixelFormat output_format = PixelFormat::U8;
};

// Determine the target resolution for a job per its `resize_mode`. Returns
// std::nullopt when no slots are populated and the mode is not Custom.
std::optional<ImageSize> compute_target_size(const PackJob& job);

struct PackResult
{
    std::optional<Image> image;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return image.has_value() && error.empty(); }
};

// Synchronously assemble the packed image. The result is always 4-channel
// at the job's output_format; missing destination channels are filled with
// 0 (RGB) or 1 (A).
PackResult pack(const PackJob& job);

} // namespace tcp
