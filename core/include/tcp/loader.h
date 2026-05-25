#pragma once

#include "tcp/image.h"

#include <filesystem>
#include <memory>
#include <string>

namespace tcp::loader {

struct LoadOptions
{
    // When true, an 8-bit input tagged as sRGB will be converted to linear
    // during decode. Default false because data textures (roughness/AO/metallic)
    // are typically authored as linear data regardless of their PNG sRGB tag.
    bool convert_srgb_to_linear = false;

    // When true, decode at 32-bit float precision regardless of source bit depth.
    // The packer works in linear float internally, so this is the recommended
    // path. Set false to retain the source format (e.g. for low-memory streaming).
    bool decode_to_float = true;
};

struct LoadResult
{
    std::shared_ptr<const Image> image;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return image != nullptr && error.empty(); }
};

// Synchronous load from disk. Returns a populated LoadResult on success; on
// failure the image is null and `error` contains a human-readable description.
LoadResult load(const std::filesystem::path& path, const LoadOptions& opts = {});

} // namespace tcp::loader
