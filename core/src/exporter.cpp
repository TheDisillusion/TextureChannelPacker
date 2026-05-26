#include "tcp/exporter.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>

#include <algorithm>
#include <cctype>
#include <string>

// Forward-declared helper from exporter_dds.cpp. Linked in only when
// TCP_ENABLE_DDS is on; otherwise we stub it to return an error.
namespace tcp::exporter::detail {
SaveResult save_dds(const Image& img, const std::filesystem::path& path,
                    const SaveOptions& opts);
} // namespace tcp::exporter::detail

namespace tcp::exporter {

namespace {

OIIO::TypeDesc to_oiio_type(PixelFormat f) noexcept
{
    switch (f) {
        case PixelFormat::U8: return OIIO::TypeDesc::UINT8;
        case PixelFormat::U16: return OIIO::TypeDesc::UINT16;
        case PixelFormat::F32: return OIIO::TypeDesc::FLOAT;
    }
    return OIIO::TypeDesc::UINT8;
}

std::string lower_extension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    if (!ext.empty() && ext.front() == '.') {
        ext.erase(ext.begin());
    }
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

} // namespace

std::optional<Format> format_from_extension(const std::filesystem::path& path)
{
    const std::string ext = lower_extension(path);
    if (ext == "png") return Format::PNG;
    if (ext == "tga") return Format::TGA;
    if (ext == "dds") return Format::DDS;
    return std::nullopt;
}

std::optional<BcVariant> bc_variant_from_string(std::string_view s)
{
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "uncompressed" || lower == "none" || lower == "raw") return BcVariant::Uncompressed;
    if (lower == "bc1" || lower == "dxt1") return BcVariant::BC1;
    if (lower == "bc3" || lower == "dxt5") return BcVariant::BC3;
    if (lower == "bc5") return BcVariant::BC5;
    if (lower == "bc7") return BcVariant::BC7;
    return std::nullopt;
}

SaveResult save(const Image& img, const std::filesystem::path& path, const SaveOptions& opts)
{
    SaveResult result;

    if (img.empty()) {
        result.error = "cannot save an empty image";
        return result;
    }

    Format fmt;
    if (opts.format) {
        fmt = *opts.format;
    } else {
        const auto inferred = format_from_extension(path);
        if (!inferred) {
            result.error = "could not infer output format from extension: " + path.string();
            return result;
        }
        fmt = *inferred;
    }

    if (fmt == Format::TGA && img.format() != PixelFormat::U8) {
        result.error = "TGA only supports 8-bit output; convert the image to U8 first";
        return result;
    }

    if (fmt == Format::DDS) {
        return detail::save_dds(img, path, opts);
    }

    auto out = OIIO::ImageOutput::create(path.string());
    if (!out) {
        result.error = OIIO::geterror();
        if (result.error.empty()) {
            result.error = "no OpenImageIO writer available for " + path.string();
        }
        return result;
    }

    const OIIO::TypeDesc tdesc = to_oiio_type(img.format());
    OIIO::ImageSpec spec(img.width(), img.height(), img.channels(), tdesc);

    // OIIO's PNG and TGA writers assume the input buffer carries associated
    // (premultiplied) alpha by default and divide RGB by A on write. Our pack
    // pipeline emits straight alpha, so without this attribute RGB gets
    // mangled whenever A != 1 (e.g. R=G=B=A=src.R collapses to R/A=1 in
    // every non-zero-alpha pixel). Tell OIIO the buffer is already
    // unassociated so it writes the bytes through unchanged.
    spec.attribute("oiio:UnassociatedAlpha", 1);

    if (fmt == Format::PNG) {
        const int level = std::clamp(opts.png_compression_level, 0, 9);
        spec.attribute("png:compressionLevel", level);
    }

    if (!out->open(path.string(), spec)) {
        result.error = out->geterror();
        if (result.error.empty()) {
            result.error = "failed to open " + path.string() + " for writing";
        }
        return result;
    }

    if (!out->write_image(tdesc, img.data())) {
        result.error = out->geterror();
        if (result.error.empty()) {
            result.error = "failed to write image data to " + path.string();
        }
        out->close();
        return result;
    }

    if (!out->close()) {
        result.error = out->geterror();
        if (result.error.empty()) {
            result.error = "failed to finalize " + path.string();
        }
        return result;
    }

    result.ok = true;
    return result;
}

} // namespace tcp::exporter
