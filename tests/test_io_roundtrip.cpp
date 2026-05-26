#include "tcp/exporter.h"
#include "tcp/image.h"
#include "tcp/loader.h"
#include "tcp/pixel_format.h"

#include "test_support.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/typedesc.h>

#include <array>
#include <filesystem>

using Catch::Matchers::WithinAbs;

namespace {

// Read the raw byte values OIIO actually stored on disk for a single pixel,
// bypassing tcp::loader entirely. We pass oiio:UnassociatedAlpha=1 in the
// open config so OIIO does not re-associate (multiply RGB by A) on read.
// This is how we tell what *was actually written* to the file, independent
// of any read-side compensation. The round-trip tests cannot tell on their
// own whether the writer is correct, because a divide-on-write + multiply-
// on-read pair cancels and the loader sees the original values either way.
struct StoredPixel
{
    bool ok = false;
    std::array<float, 4> rgba{};
};

StoredPixel read_disk_pixel(const std::filesystem::path& path, int x, int y)
{
    StoredPixel out;
    OIIO::ImageSpec config;
    config.attribute("oiio:UnassociatedAlpha", 1);

    auto in = OIIO::ImageInput::open(path.string(), &config);
    if (!in) {
        return out;
    }
    const OIIO::ImageSpec spec = in->spec();
    if (spec.nchannels < 4 || x < 0 || y < 0 || x >= spec.width || y >= spec.height) {
        return out;
    }
    std::vector<float> buf(static_cast<std::size_t>(spec.width) * spec.height * spec.nchannels);
    if (!in->read_image(0, 0, 0, spec.nchannels, OIIO::TypeDesc::FLOAT, buf.data())) {
        return out;
    }
    const std::size_t base = (static_cast<std::size_t>(y) * spec.width + x) * spec.nchannels;
    out.rgba = {buf[base + 0], buf[base + 1], buf[base + 2], buf[base + 3]};
    out.ok = true;
    return out;
}

} // namespace

TEST_CASE("exporter infers format from extension", "[exporter]")
{
    using tcp::exporter::Format;
    using tcp::exporter::format_from_extension;

    REQUIRE(format_from_extension("foo.png") == Format::PNG);
    REQUIRE(format_from_extension("FOO.PNG") == Format::PNG);
    REQUIRE(format_from_extension("foo.tga") == Format::TGA);
    REQUIRE_FALSE(format_from_extension("foo.bmp").has_value());
    REQUIRE_FALSE(format_from_extension("foo").has_value());
}

TEST_CASE("exporter rejects an empty image", "[exporter][error]")
{
    const tcp::Image empty;
    auto out = tcp::exporter::save(empty, tcp::test::scratch_path("empty.png"));
    REQUIRE_FALSE(out.ok);
}

TEST_CASE("exporter rejects TGA + non-U8", "[exporter][error]")
{
    const auto img = tcp::test::make_solid(8, 8, tcp::PixelFormat::F32, 1.0f, 0.0f, 0.0f, 1.0f);
    auto out = tcp::exporter::save(img, tcp::test::scratch_path("bad.tga"));
    REQUIRE_FALSE(out.ok);
}

TEST_CASE("PNG round-trip preserves pixel values within U8 quantization",
          "[exporter][loader][roundtrip][png]")
{
    const auto src = tcp::test::make_solid(8, 8, tcp::PixelFormat::U8,
                                           0.25f, 0.5f, 0.75f, 1.0f);
    const auto path = tcp::test::scratch_path("rgba_solid_roundtrip.png");
    std::filesystem::remove(path);

    auto save_res = tcp::exporter::save(src, path);
    INFO(save_res.error);
    REQUIRE(save_res.ok);
    REQUIRE(std::filesystem::exists(path));

    auto load_res = tcp::loader::load(path);
    INFO(load_res.error);
    REQUIRE(load_res.ok());
    REQUIRE(load_res.image->width() == 8);
    REQUIRE(load_res.image->height() == 8);
    REQUIRE(load_res.image->channels() == 4);

    const float tol = tcp::test::u8_round_trip_tolerance;
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 0), WithinAbs(0.25f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 1), WithinAbs(0.5f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 2), WithinAbs(0.75f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 3), WithinAbs(1.0f, tol));
}

TEST_CASE("TGA round-trip preserves pixel values within U8 quantization",
          "[exporter][loader][roundtrip][tga]")
{
    const auto src = tcp::test::make_solid(4, 4, tcp::PixelFormat::U8,
                                           1.0f, 0.0f, 0.0f, 1.0f);
    const auto path = tcp::test::scratch_path("rgba_solid_roundtrip.tga");
    std::filesystem::remove(path);

    auto save_res = tcp::exporter::save(src, path);
    INFO(save_res.error);
    REQUIRE(save_res.ok);

    auto load_res = tcp::loader::load(path);
    INFO(load_res.error);
    REQUIRE(load_res.ok());

    const float tol = tcp::test::u8_round_trip_tolerance;
    REQUIRE_THAT(load_res.image->sample_linear(2, 2, 0), WithinAbs(1.0f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(2, 2, 1), WithinAbs(0.0f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(2, 2, 2), WithinAbs(0.0f, tol));
}

// Regression guards for OIIO's default assumption that 4-channel buffers
// carry associated (premultiplied) alpha. Our pack pipeline always emits
// straight alpha; without oiio:UnassociatedAlpha=1 on the spec, OIIO divides
// RGB by A on write, which only shows up when A != 1.0 — and every earlier
// 4-channel test used A=1, masking the bug.
TEST_CASE("PNG round-trip with A != 1 preserves all four channels",
          "[exporter][loader][roundtrip][png][alpha]")
{
    const auto src = tcp::test::make_solid(8, 8, tcp::PixelFormat::U8,
                                           0.25f, 0.5f, 0.75f, 0.5f);
    const auto path = tcp::test::scratch_path("rgba_unassoc_roundtrip.png");
    std::filesystem::remove(path);

    auto save_res = tcp::exporter::save(src, path);
    INFO(save_res.error);
    REQUIRE(save_res.ok);

    auto load_res = tcp::loader::load(path);
    INFO(load_res.error);
    REQUIRE(load_res.ok());
    REQUIRE(load_res.image->channels() == 4);

    const float tol = tcp::test::u8_round_trip_tolerance;
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 0), WithinAbs(0.25f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 1), WithinAbs(0.5f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 2), WithinAbs(0.75f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 3), WithinAbs(0.5f, tol));
}

TEST_CASE("TGA round-trip with A != 1 preserves all four channels",
          "[exporter][loader][roundtrip][tga][alpha]")
{
    const auto src = tcp::test::make_solid(4, 4, tcp::PixelFormat::U8,
                                           0.25f, 0.5f, 0.75f, 0.5f);
    const auto path = tcp::test::scratch_path("rgba_unassoc_roundtrip.tga");
    std::filesystem::remove(path);

    auto save_res = tcp::exporter::save(src, path);
    INFO(save_res.error);
    REQUIRE(save_res.ok);

    auto load_res = tcp::loader::load(path);
    INFO(load_res.error);
    REQUIRE(load_res.ok());
    REQUIRE(load_res.image->channels() == 4);

    const float tol = tcp::test::u8_round_trip_tolerance;
    REQUIRE_THAT(load_res.image->sample_linear(2, 2, 0), WithinAbs(0.25f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(2, 2, 1), WithinAbs(0.5f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(2, 2, 2), WithinAbs(0.75f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(2, 2, 3), WithinAbs(0.5f, tol));
}

// Verify what is actually on disk after exporter::save, without going through
// our loader. Read directly with OIIO using oiio:UnassociatedAlpha=1 so the
// reader does not silently undo any premultiplication the writer applied.
// This is the strict regression guard: if the writer ever drops the
// UnassociatedAlpha attribute again, RGB will come back at R/A on disk and
// these tests fail outright.
TEST_CASE("PNG export writes straight (unassociated) alpha to disk",
          "[exporter][png][alpha][ondisk]")
{
    const auto src = tcp::test::make_solid(8, 8, tcp::PixelFormat::U8,
                                           0.25f, 0.5f, 0.75f, 0.5f);
    const auto path = tcp::test::scratch_path("rgba_on_disk_check.png");
    std::filesystem::remove(path);

    auto save_res = tcp::exporter::save(src, path);
    INFO(save_res.error);
    REQUIRE(save_res.ok);

    const auto px = read_disk_pixel(path, 4, 4);
    REQUIRE(px.ok);

    const float tol = tcp::test::u8_round_trip_tolerance;
    REQUIRE_THAT(px.rgba[0], WithinAbs(0.25f, tol));
    REQUIRE_THAT(px.rgba[1], WithinAbs(0.5f, tol));
    REQUIRE_THAT(px.rgba[2], WithinAbs(0.75f, tol));
    REQUIRE_THAT(px.rgba[3], WithinAbs(0.5f, tol));
}

TEST_CASE("TGA export writes straight (unassociated) alpha to disk",
          "[exporter][tga][alpha][ondisk]")
{
    const auto src = tcp::test::make_solid(4, 4, tcp::PixelFormat::U8,
                                           0.25f, 0.5f, 0.75f, 0.5f);
    const auto path = tcp::test::scratch_path("rgba_on_disk_check.tga");
    std::filesystem::remove(path);

    auto save_res = tcp::exporter::save(src, path);
    INFO(save_res.error);
    REQUIRE(save_res.ok);

    const auto px = read_disk_pixel(path, 2, 2);
    REQUIRE(px.ok);

    const float tol = tcp::test::u8_round_trip_tolerance;
    REQUIRE_THAT(px.rgba[0], WithinAbs(0.25f, tol));
    REQUIRE_THAT(px.rgba[1], WithinAbs(0.5f, tol));
    REQUIRE_THAT(px.rgba[2], WithinAbs(0.75f, tol));
    REQUIRE_THAT(px.rgba[3], WithinAbs(0.5f, tol));
}
