#include "tcp/loader.h"

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>

#include <cmath>
#include <cstddef>

namespace tcp::loader {

namespace {

constexpr float srgb_to_linear(float c) noexcept
{
    if (c <= 0.04045f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

} // namespace

LoadResult load(const std::filesystem::path& path, const LoadOptions& opts)
{
    LoadResult result;

    auto in = OIIO::ImageInput::open(path.string());
    if (!in) {
        result.error = OIIO::geterror();
        if (result.error.empty()) {
            result.error = "failed to open " + path.string();
        }
        return result;
    }

    const OIIO::ImageSpec spec = in->spec();
    const int width = spec.width;
    const int height = spec.height;
    const int channels = spec.nchannels;

    if (width <= 0 || height <= 0 || channels <= 0) {
        result.error = "image has zero extent or channel count";
        return result;
    }

    if (channels > 4) {
        result.error = "images with more than 4 channels are not supported";
        return result;
    }

    Image img(width, height, channels, opts.decode_to_float ? PixelFormat::F32 : PixelFormat::U8);

    const OIIO::TypeDesc dest_type = opts.decode_to_float ? OIIO::TypeDesc::FLOAT : OIIO::TypeDesc::UINT8;
    const bool ok = in->read_image(0, 0, 0, channels, dest_type, img.data());
    if (!ok) {
        result.error = in->geterror();
        if (result.error.empty()) {
            result.error = "failed to decode pixels from " + path.string();
        }
        return result;
    }

    if (opts.convert_srgb_to_linear && opts.decode_to_float) {
        const int color_channels = (channels >= 3) ? 3 : channels;
        const std::size_t pixel_count = static_cast<std::size_t>(width)
                                        * static_cast<std::size_t>(height);
        auto* pixels = reinterpret_cast<float*>(img.data());
        for (std::size_t i = 0; i < pixel_count; ++i) {
            for (int k = 0; k < color_channels; ++k) {
                pixels[i * static_cast<std::size_t>(channels) + static_cast<std::size_t>(k)]
                    = srgb_to_linear(pixels[i * static_cast<std::size_t>(channels)
                                            + static_cast<std::size_t>(k)]);
            }
        }
    }

    result.image = std::make_shared<const Image>(std::move(img));
    return result;
}

} // namespace tcp::loader
