#pragma once

#include "tcp/image.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace tcp::exporter {

enum class Format : std::uint8_t {
    PNG,
    TGA,
};

constexpr std::string_view to_string(Format f) noexcept
{
    switch (f) {
        case Format::PNG: return "png";
        case Format::TGA: return "tga";
    }
    return "png";
}

struct SaveOptions
{
    // If unset, the format is inferred from the path extension.
    std::optional<Format> format;

    // Maps to libpng compression for PNG. 0 = no compression, 9 = max. Ignored for TGA.
    int png_compression_level = 6;
};

struct SaveResult
{
    bool ok = false;
    std::string error;
};

// Persist an image to disk. The image's pixel format is preserved (U8/U16/F32),
// with the writer expected to handle the bit-depth choice per file format
// (PNG: U8 and U16 supported; TGA: U8 only).
SaveResult save(const Image& img, const std::filesystem::path& path, const SaveOptions& opts = {});

// Infer the format from the extension. Returns std::nullopt for unknown ones.
std::optional<Format> format_from_extension(const std::filesystem::path& path);

} // namespace tcp::exporter
