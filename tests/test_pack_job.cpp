#include "tcp/channel_ref.h"
#include "tcp/exporter.h"
#include "tcp/image.h"
#include "tcp/loader.h"
#include "tcp/pack_job.h"

#include "test_support.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using Catch::Matchers::WithinAbs;

namespace {

std::shared_ptr<const tcp::Image> solid_rgba(int w, int h, float r, float g, float b, float a)
{
    return std::make_shared<const tcp::Image>(
        tcp::test::make_solid(w, h, tcp::PixelFormat::U8, r, g, b, a));
}

} // namespace

TEST_CASE("compute_target_size picks the largest populated input by default",
          "[pack_job][resize_mode]")
{
    tcp::PackJob job;
    job.resize_mode = tcp::ResizeMode::Largest;
    job.inputs[0].image = solid_rgba(64, 64, 1, 0, 0, 1);
    job.inputs[1].image = solid_rgba(128, 128, 0, 1, 0, 1);
    job.inputs[2].image = solid_rgba(32, 32, 0, 0, 1, 1);

    const auto size = tcp::compute_target_size(job);
    REQUIRE(size.has_value());
    REQUIRE(size->width == 128);
    REQUIRE(size->height == 128);
}

TEST_CASE("compute_target_size honors Smallest and FirstPopulated",
          "[pack_job][resize_mode]")
{
    tcp::PackJob job;
    job.inputs[0].image = solid_rgba(64, 64, 1, 0, 0, 1);
    job.inputs[1].image = solid_rgba(128, 128, 0, 1, 0, 1);

    job.resize_mode = tcp::ResizeMode::Smallest;
    auto small = tcp::compute_target_size(job);
    REQUIRE(small.has_value());
    REQUIRE(small->width == 64);

    job.resize_mode = tcp::ResizeMode::FirstPopulated;
    auto first = tcp::compute_target_size(job);
    REQUIRE(first.has_value());
    REQUIRE(first->width == 64);
}

TEST_CASE("compute_target_size returns nullopt when no slots are populated",
          "[pack_job][resize_mode]")
{
    tcp::PackJob job;
    job.resize_mode = tcp::ResizeMode::Largest;
    REQUIRE_FALSE(tcp::compute_target_size(job).has_value());
}

TEST_CASE("compute_target_size returns the custom value when mode is Custom",
          "[pack_job][resize_mode]")
{
    tcp::PackJob job;
    job.resize_mode = tcp::ResizeMode::Custom;
    job.custom_size = {512, 256};
    auto size = tcp::compute_target_size(job);
    REQUIRE(size.has_value());
    REQUIRE(size->width == 512);
    REQUIRE(size->height == 256);
}

TEST_CASE("pack assembles the four channel inputs into one packed image",
          "[pack_job][pack]")
{
    // Slot 0: red plate, source R goes into destination R.
    // Slot 1: green plate, source G goes into destination G.
    // Slot 2: blue plate, source B goes into destination B.
    // Slot 3: white plate, source R goes into destination A.
    tcp::PackJob job;
    job.inputs[0].image = solid_rgba(16, 16, 1.0f, 0.0f, 0.0f, 1.0f);
    job.inputs[1].image = solid_rgba(16, 16, 0.0f, 1.0f, 0.0f, 1.0f);
    job.inputs[2].image = solid_rgba(16, 16, 0.0f, 0.0f, 1.0f, 1.0f);
    job.inputs[3].image = solid_rgba(16, 16, 1.0f, 1.0f, 1.0f, 1.0f);
    job.channel_map[0] = {0, tcp::SourceChannel::R};
    job.channel_map[1] = {1, tcp::SourceChannel::G};
    job.channel_map[2] = {2, tcp::SourceChannel::B};
    job.channel_map[3] = {3, tcp::SourceChannel::R};

    auto packed = tcp::pack(job);
    INFO(packed.error);
    REQUIRE(packed.ok());
    REQUIRE(packed.image->width() == 16);
    REQUIRE(packed.image->height() == 16);
    REQUIRE(packed.image->channels() == 4);

    const float tol = tcp::test::u8_round_trip_tolerance;
    for (int y = 0; y < 16; y += 5) {
        for (int x = 0; x < 16; x += 5) {
            REQUIRE_THAT(packed.image->sample_linear(x, y, 0), WithinAbs(1.0f, tol));
            REQUIRE_THAT(packed.image->sample_linear(x, y, 1), WithinAbs(1.0f, tol));
            REQUIRE_THAT(packed.image->sample_linear(x, y, 2), WithinAbs(1.0f, tol));
            REQUIRE_THAT(packed.image->sample_linear(x, y, 3), WithinAbs(1.0f, tol));
        }
    }
}

TEST_CASE("pack fills unset destination channels with the appropriate default",
          "[pack_job][defaults]")
{
    tcp::PackJob job;
    job.inputs[0].image = solid_rgba(8, 8, 0.5f, 0.5f, 0.5f, 1.0f);
    job.channel_map[0] = {0, tcp::SourceChannel::R};
    // G, B, A intentionally unset.

    auto packed = tcp::pack(job);
    INFO(packed.error);
    REQUIRE(packed.ok());

    const float tol = tcp::test::u8_round_trip_tolerance;
    REQUIRE_THAT(packed.image->sample_linear(4, 4, 0), WithinAbs(0.5f, tol));
    REQUIRE_THAT(packed.image->sample_linear(4, 4, 1), WithinAbs(0.0f, tol)); // RGB default 0
    REQUIRE_THAT(packed.image->sample_linear(4, 4, 2), WithinAbs(0.0f, tol));
    REQUIRE_THAT(packed.image->sample_linear(4, 4, 3), WithinAbs(1.0f, tol)); // alpha default 1
}

TEST_CASE("pack resizes mismatched inputs to the largest target",
          "[pack_job][resize]")
{
    tcp::PackJob job;
    job.inputs[0].image = solid_rgba(32, 32, 1.0f, 0.0f, 0.0f, 1.0f);
    job.inputs[1].image = solid_rgba(64, 64, 0.0f, 1.0f, 0.0f, 1.0f);
    job.channel_map[0] = {0, tcp::SourceChannel::R};
    job.channel_map[1] = {1, tcp::SourceChannel::G};

    auto packed = tcp::pack(job);
    INFO(packed.error);
    REQUIRE(packed.ok());
    REQUIRE(packed.image->width() == 64);
    REQUIRE(packed.image->height() == 64);
}

TEST_CASE("pack routes luminance correctly", "[pack_job][luminance]")
{
    // Solid green: luminance ~= 0.7152.
    tcp::PackJob job;
    job.inputs[0].image = solid_rgba(4, 4, 0.0f, 1.0f, 0.0f, 1.0f);
    job.channel_map[0] = {0, tcp::SourceChannel::Luminance};

    auto packed = tcp::pack(job);
    INFO(packed.error);
    REQUIRE(packed.ok());
    REQUIRE_THAT(packed.image->sample_linear(2, 2, 0),
                 WithinAbs(0.7152f, tcp::test::u8_round_trip_tolerance));
}

TEST_CASE("pack -> save -> load preserves the packed image",
          "[pack_job][roundtrip]")
{
    tcp::PackJob job;
    job.inputs[0].image = solid_rgba(8, 8, 0.25f, 0.0f, 0.0f, 1.0f);
    job.inputs[1].image = solid_rgba(8, 8, 0.0f, 0.5f, 0.0f, 1.0f);
    job.inputs[2].image = solid_rgba(8, 8, 0.0f, 0.0f, 0.75f, 1.0f);
    job.channel_map[0] = {0, tcp::SourceChannel::R};
    job.channel_map[1] = {1, tcp::SourceChannel::G};
    job.channel_map[2] = {2, tcp::SourceChannel::B};

    auto packed = tcp::pack(job);
    INFO(packed.error);
    REQUIRE(packed.ok());

    const auto path = tcp::test::scratch_path("packed_roundtrip.png");
    std::filesystem::remove(path);

    auto save_res = tcp::exporter::save(*packed.image, path);
    INFO(save_res.error);
    REQUIRE(save_res.ok);

    auto load_res = tcp::loader::load(path);
    INFO(load_res.error);
    REQUIRE(load_res.ok());

    const float tol = tcp::test::u8_round_trip_tolerance;
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 0), WithinAbs(0.25f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 1), WithinAbs(0.5f, tol));
    REQUIRE_THAT(load_res.image->sample_linear(4, 4, 2), WithinAbs(0.75f, tol));
}
