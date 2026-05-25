// DDS exporter for tcp::core. Built only when TCP_ENABLE_DDS is on; the
// dispatch in exporter.cpp falls through to a stub otherwise.
//
// We require the input to be 4-channel U8. The pack pipeline always emits
// 4-channel output (R/G/B/A) and the GUI/CLI both default to U8 for DDS, so
// this constraint is harmless. Float/16-bit DDS is feasible via DXGI_FORMAT_*
// but adds another permutation that is best left to a future iteration.

#include "tcp/exporter.h"
#include "tcp/image.h"

#ifndef TCP_HAVE_DDS

namespace tcp::exporter::detail {
SaveResult save_dds(const Image&, const std::filesystem::path&, const SaveOptions&)
{
    return SaveResult{false, "DDS support not compiled in (rebuild with TCP_ENABLE_DDS=ON)"};
}
} // namespace tcp::exporter::detail

#else // TCP_HAVE_DDS

#include <DirectXTex.h>

#include <cstddef>
#include <cstring>

namespace tcp::exporter::detail {

namespace {

// Map our BC variant onto the DXGI_FORMAT used by both the destination image
// and the DirectXTex compressor.
DXGI_FORMAT bc_target_format(BcVariant b) noexcept
{
    switch (b) {
        case BcVariant::BC1: return DXGI_FORMAT_BC1_UNORM;
        case BcVariant::BC3: return DXGI_FORMAT_BC3_UNORM;
        case BcVariant::BC5: return DXGI_FORMAT_BC5_UNORM;
        case BcVariant::BC7: return DXGI_FORMAT_BC7_UNORM;
        case BcVariant::Uncompressed: return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    return DXGI_FORMAT_R8G8B8A8_UNORM;
}

bool requires_alpha(BcVariant b) noexcept
{
    return b == BcVariant::BC3 || b == BcVariant::BC7;
}

// Bring the input to a 4-channel U8 byte buffer regardless of source layout.
// Returns the buffer plus stride; the buffer is moved into a vector kept
// alive by the caller.
struct Rgba8Buffer
{
    std::vector<std::uint8_t> bytes;
    std::size_t row_pitch = 0;
};

Rgba8Buffer to_rgba8(const Image& img)
{
    Rgba8Buffer out;
    const std::size_t w = static_cast<std::size_t>(img.width());
    const std::size_t h = static_cast<std::size_t>(img.height());
    out.row_pitch = w * 4;
    out.bytes.resize(out.row_pitch * h);

    if (img.format() == PixelFormat::U8 && img.channels() == 4) {
        std::memcpy(out.bytes.data(), img.data(), out.bytes.size());
        return out;
    }

    // Slow path: convert via sample_linear, which handles any source layout.
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const std::size_t base = static_cast<std::size_t>(y) * out.row_pitch
                                     + static_cast<std::size_t>(x) * 4;
            for (int c = 0; c < 4; ++c) {
                float v = img.sample_linear(x, y, c);
                v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                out.bytes[base + static_cast<std::size_t>(c)]
                    = static_cast<std::uint8_t>(v * 255.0f + 0.5f);
            }
        }
    }
    return out;
}

std::string hresult_to_string(HRESULT hr)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "HRESULT 0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

} // namespace

SaveResult save_dds(const Image& img, const std::filesystem::path& path,
                    const SaveOptions& opts)
{
    SaveResult result;

    if (img.empty()) {
        result.error = "cannot save an empty image";
        return result;
    }

    const BcVariant variant = opts.bc_variant;

    // BC1/3/5/7 require dimensions that are multiples of 4. Many real
    // textures are POT and this just works, but flag the failure clearly
    // when it doesn't.
    if (variant != BcVariant::Uncompressed) {
        if ((img.width() % 4) != 0 || (img.height() % 4) != 0) {
            result.error
                = "BC compression requires width and height to be multiples of 4 "
                  "(got " + std::to_string(img.width()) + "x" + std::to_string(img.height()) + ")";
            return result;
        }
    }

    Rgba8Buffer rgba = to_rgba8(img);

    DirectX::Image src_image{};
    src_image.width = static_cast<std::size_t>(img.width());
    src_image.height = static_cast<std::size_t>(img.height());
    src_image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    src_image.rowPitch = rgba.row_pitch;
    src_image.slicePitch = rgba.bytes.size();
    src_image.pixels = rgba.bytes.data();

    DirectX::ScratchImage scratch;
    DirectX::ScratchImage compressed;

    const DXGI_FORMAT target = bc_target_format(variant);

    // BC5/BC1/BC3 can run on the CPU; BC7 has both CPU and GPU paths. We
    // stay on CPU for portability (no D3D11 device needed at runtime).
    DirectX::TEX_COMPRESS_FLAGS compress_flags = DirectX::TEX_COMPRESS_DEFAULT;
    if (requires_alpha(variant)) {
        compress_flags |= DirectX::TEX_COMPRESS_RGB_DITHER;
    }

    if (variant == BcVariant::Uncompressed) {
        // Direct write: wrap the buffer in a ScratchImage and save.
        HRESULT hr = scratch.InitializeFromImage(src_image);
        if (FAILED(hr)) {
            result.error = "InitializeFromImage failed: " + hresult_to_string(hr);
            return result;
        }
        hr = DirectX::SaveToDDSFile(*scratch.GetImage(0, 0, 0), DirectX::DDS_FLAGS_NONE,
                                    path.c_str());
        if (FAILED(hr)) {
            result.error = "SaveToDDSFile failed: " + hresult_to_string(hr);
            return result;
        }
    } else {
        // Encode via DirectXTex's Compress. The 4-channel uncompressed source
        // is required input regardless of the BC variant.
        HRESULT hr = DirectX::Compress(src_image, target, compress_flags,
                                       DirectX::TEX_THRESHOLD_DEFAULT, compressed);
        if (FAILED(hr)) {
            result.error = "DirectX::Compress (" + std::string(to_string(variant))
                           + ") failed: " + hresult_to_string(hr);
            return result;
        }
        hr = DirectX::SaveToDDSFile(*compressed.GetImage(0, 0, 0), DirectX::DDS_FLAGS_NONE,
                                    path.c_str());
        if (FAILED(hr)) {
            result.error = "SaveToDDSFile failed: " + hresult_to_string(hr);
            return result;
        }
    }

    result.ok = true;
    return result;
}

} // namespace tcp::exporter::detail

#endif // TCP_HAVE_DDS
