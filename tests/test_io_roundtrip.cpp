#include "tcp/exporter.h"
#include "tcp/image.h"
#include "tcp/loader.h"
#include "tcp/pixel_format.h"

#include "test_support.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <filesystem>

using Catch::Matchers::WithinAbs;

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
