#include "tcp/image.h"
#include "tcp/pixel_format.h"

#include "test_support.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE("default Image is empty", "[image]")
{
    tcp::Image img;
    REQUIRE(img.empty());
    REQUIRE(img.width() == 0);
    REQUIRE(img.height() == 0);
    REQUIRE(img.byte_size() == 0);
}

TEST_CASE("Image constructor allocates the correct number of bytes", "[image]")
{
    const tcp::Image u8(64, 32, 4, tcp::PixelFormat::U8);
    REQUIRE(u8.byte_size() == 64u * 32u * 4u * 1u);
    REQUIRE(u8.row_stride_bytes() == 64u * 4u * 1u);

    const tcp::Image u16(32, 32, 3, tcp::PixelFormat::U16);
    REQUIRE(u16.byte_size() == 32u * 32u * 3u * 2u);

    const tcp::Image f32(16, 16, 1, tcp::PixelFormat::F32);
    REQUIRE(f32.byte_size() == 16u * 16u * 1u * 4u);
}

TEST_CASE("Image clamps invalid channel counts", "[image]")
{
    const tcp::Image too_many(8, 8, 7, tcp::PixelFormat::U8);
    REQUIRE(too_many.channels() == 4);

    const tcp::Image too_few(8, 8, 0, tcp::PixelFormat::U8);
    REQUIRE(too_few.channels() == 1);
}

TEST_CASE("U8 round-trips through set_linear/sample_linear within quantization", "[image][u8]")
{
    tcp::Image img(2, 1, 1, tcp::PixelFormat::U8);
    img.set_linear(0, 0, 0, 0.0f);
    img.set_linear(1, 0, 0, 1.0f);
    REQUIRE_THAT(img.sample_linear(0, 0, 0), WithinAbs(0.0f, 0.5f / 255.0f));
    REQUIRE_THAT(img.sample_linear(1, 0, 0), WithinAbs(1.0f, 0.5f / 255.0f));

    img.set_linear(0, 0, 0, 0.5f);
    REQUIRE_THAT(img.sample_linear(0, 0, 0), WithinAbs(0.5f, tcp::test::u8_round_trip_tolerance));
}

TEST_CASE("F32 stores values verbatim without clamping", "[image][f32]")
{
    tcp::Image img(1, 1, 1, tcp::PixelFormat::F32);
    img.set_linear(0, 0, 0, 2.5f);
    REQUIRE(img.sample_linear(0, 0, 0) == 2.5f);

    img.set_linear(0, 0, 0, -0.5f);
    REQUIRE(img.sample_linear(0, 0, 0) == -0.5f);
}

TEST_CASE("sample_linear clamps out-of-bounds coordinates", "[image]")
{
    auto img = tcp::test::make_horizontal_ramp(4, 1, tcp::PixelFormat::U8);
    const float right_edge = img.sample_linear(3, 0, 0);
    REQUIRE_THAT(img.sample_linear(100, 0, 0), WithinAbs(right_edge, 1e-6f));
    const float left_edge = img.sample_linear(0, 0, 0);
    REQUIRE_THAT(img.sample_linear(-5, 0, 0), WithinAbs(left_edge, 1e-6f));
}
