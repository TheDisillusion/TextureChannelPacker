#include "tcp/resize.h"

#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/typedesc.h>

#include <cstring>
#include <string>

namespace tcp {

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

// OIIO does not accept "nearest" or "bilinear" as filter names. It uses
// "box" (with width 1.0) and "triangle" respectively. Keeping the mapping in
// one place protects the public ResizeFilter enum from leaking OIIO terms.
struct OiioFilter
{
    const char* name;
    float width;
};

constexpr OiioFilter to_oiio_filter(ResizeFilter f) noexcept
{
    switch (f) {
        case ResizeFilter::Nearest: return {"box", 1.0f};
        case ResizeFilter::Bilinear: return {"triangle", 0.0f};
        case ResizeFilter::Mitchell: return {"mitchell", 0.0f};
        case ResizeFilter::Lanczos: return {"lanczos3", 0.0f};
    }
    return {"mitchell", 0.0f};
}

} // namespace

ResizeResult resize(const Image& src, int new_width, int new_height, ResizeFilter filter)
{
    ResizeResult result;

    if (src.empty()) {
        result.error = "source image is empty";
        return result;
    }
    if (new_width <= 0 || new_height <= 0) {
        result.error = "target size must be positive";
        return result;
    }

    if (new_width == src.width() && new_height == src.height()) {
        Image copy(src.width(), src.height(), src.channels(), src.format());
        std::memcpy(copy.data(), src.data(), src.byte_size());
        result.image = std::move(copy);
        return result;
    }

    const OIIO::TypeDesc tdesc = to_oiio_type(src.format());

    OIIO::ImageSpec src_spec(src.width(), src.height(), src.channels(), tdesc);
    OIIO::ImageBuf src_buf(src_spec, const_cast<std::byte*>(src.data()));

    const OiioFilter oiio_filter = to_oiio_filter(filter);
    const OIIO::ROI dst_roi(0, new_width, 0, new_height, 0, 1, 0, src.channels());
    OIIO::ImageBuf dst_buf
        = OIIO::ImageBufAlgo::resize(src_buf, oiio_filter.name, oiio_filter.width, dst_roi);

    if (dst_buf.has_error()) {
        result.error = dst_buf.geterror();
        if (result.error.empty()) {
            result.error = "OpenImageIO resize failed";
        }
        return result;
    }

    Image out(new_width, new_height, src.channels(), src.format());
    const bool got = dst_buf.get_pixels(dst_roi, tdesc, out.data());
    if (!got) {
        result.error = dst_buf.geterror();
        if (result.error.empty()) {
            result.error = "OpenImageIO get_pixels failed";
        }
        return result;
    }

    result.image = std::move(out);
    return result;
}

} // namespace tcp
