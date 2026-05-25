#include "tcp/image.h"
#include "tcp/resize.h"

#include "test_support.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("resize to the same size copies pixels exactly", "[resize][identity]")
{
    const auto src = tcp::test::make_horizontal_ramp(8, 8, tcp::PixelFormat::U8);
    auto result = tcp::resize(src, 8, 8, tcp::ResizeFilter::Mitchell);
    REQUIRE(result.ok());
    REQUIRE(result.image.width() == 8);
    REQUIRE(result.image.height() == 8);
    REQUIRE(result.image.byte_size() == src.byte_size());
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            REQUIRE(result.image.sample_linear(x, y, 0) == src.sample_linear(x, y, 0));
        }
    }
}

TEST_CASE("resize produces an image of the requested dimensions", "[resize]")
{
    const auto src = tcp::test::make_horizontal_ramp(16, 16, tcp::PixelFormat::F32);

    SECTION("downsample")
    {
        auto result = tcp::resize(src, 4, 4, tcp::ResizeFilter::Mitchell);
        REQUIRE(result.ok());
        REQUIRE(result.image.width() == 4);
        REQUIRE(result.image.height() == 4);
        REQUIRE(result.image.channels() == 1);
        REQUIRE(result.image.format() == tcp::PixelFormat::F32);
    }
    SECTION("upsample")
    {
        auto result = tcp::resize(src, 64, 64, tcp::ResizeFilter::Bilinear);
        REQUIRE(result.ok());
        REQUIRE(result.image.width() == 64);
        REQUIRE(result.image.height() == 64);
    }
}

TEST_CASE("resize preserves rough intensity range", "[resize]")
{
    // A horizontal ramp resized 4x should still go from ~0 on the left to ~1 on the right.
    const auto src = tcp::test::make_horizontal_ramp(32, 1, tcp::PixelFormat::F32);
    auto result = tcp::resize(src, 128, 1, tcp::ResizeFilter::Mitchell);
    REQUIRE(result.ok());
    REQUIRE_THAT(result.image.sample_linear(0, 0, 0), WithinAbs(0.0f, 0.05f));
    REQUIRE_THAT(result.image.sample_linear(127, 0, 0), WithinAbs(1.0f, 0.05f));
}

TEST_CASE("resize rejects an empty source", "[resize][error]")
{
    const tcp::Image empty;
    auto result = tcp::resize(empty, 4, 4, tcp::ResizeFilter::Nearest);
    REQUIRE_FALSE(result.ok());
    REQUIRE_FALSE(result.error.empty());
}

TEST_CASE("resize rejects a zero or negative target size", "[resize][error]")
{
    const auto src = tcp::test::make_horizontal_ramp(4, 4, tcp::PixelFormat::U8);
    auto result = tcp::resize(src, 0, 4, tcp::ResizeFilter::Nearest);
    REQUIRE_FALSE(result.ok());
}
