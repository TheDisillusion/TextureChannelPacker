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
    DDS,
};

// BC variant for DDS output. Ignored when format is PNG or TGA.
enum class BcVariant : std::uint8_t {
    // No BC compression — store raw RGBA8 inside the DDS container. Lossless.
    Uncompressed,
    // 4 bpp, 1-bit alpha. Best for opaque color maps.
    BC1,
    // 8 bpp, color + 8-bit alpha. Best for diffuse with alpha.
    BC3,
    // 8 bpp, two channels (R, G). Designed for tangent-space normal maps.
    BC5,
    // 8 bpp, high quality color + alpha. Slowest to encode, best quality.
    BC7,
};

constexpr std::string_view to_string(Format f) noexcept
{
    switch (f) {
        case Format::PNG: return "png";
        case Format::TGA: return "tga";
        case Format::DDS: return "dds";
    }
    return "png";
}

constexpr std::string_view to_string(BcVariant b) noexcept
{
    switch (b) {
        case BcVariant::Uncompressed: return "uncompressed";
        case BcVariant::BC1: return "bc1";
        case BcVariant::BC3: return "bc3";
        case BcVariant::BC5: return "bc5";
        case BcVariant::BC7: return "bc7";
    }
    return "uncompressed";
}

struct SaveOptions
{
    // If unset, the format is inferred from the path extension.
    std::optional<Format> format;

    // Maps to libpng compression for PNG. 0 = no compression, 9 = max. Ignored for TGA / DDS.
    int png_compression_level = 6;

    // Only consulted when format == DDS.
    BcVariant bc_variant = BcVariant::BC7;

    // When true and format == DDS, write the DDS bottom-up (last source row
    // first). Engines that don't re-orient DDS on import (Unity and other
    // GL-UV-convention consumers) will then display the texture right-side-up.
    // Default false so D3D-native tools and Unreal get standard top-down DDS.
    // Ignored for PNG / TGA — those formats are correctly re-oriented by
    // every engine importer we care about.
    bool flip_vertical = false;
};

struct SaveResult
{
    bool ok = false;
    std::string error;
};

// Persist an image to disk. The image's pixel format is preserved (U8/U16/F32),
// with the writer expected to handle the bit-depth choice per file format
// (PNG: U8 and U16 supported; TGA: U8 only; DDS: U8 only for this revision).
SaveResult save(const Image& img, const std::filesystem::path& path, const SaveOptions& opts = {});

// Infer the format from the extension. Returns std::nullopt for unknown ones.
std::optional<Format> format_from_extension(const std::filesystem::path& path);

// Parse a CLI-style BC variant name. Case-insensitive; accepts "uncompressed",
// "bc1", "bc3", "bc5", "bc7". Returns std::nullopt on unknown input.
std::optional<BcVariant> bc_variant_from_string(std::string_view s);

} // namespace tcp::exporter
