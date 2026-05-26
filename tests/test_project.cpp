#include "tcp/channel_ref.h"
#include "tcp/exporter.h"
#include "tcp/pack_job.h"
#include "tcp/pixel_format.h"
#include "tcp/project.h"
#include "tcp/resize.h"

#include "test_support.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

tcp::project::Project make_sample_project()
{
    tcp::project::Project p;
    p.job.inputs[0].path = "C:/textures/ao.png";
    p.job.inputs[0].treat_as_srgb = false;
    p.job.inputs[1].path = "C:/textures/roughness.png";
    p.job.inputs[1].treat_as_srgb = false;
    p.job.inputs[2].path = "/usr/local/share/metallic.tga";
    p.job.inputs[2].treat_as_srgb = true;
    // slot 3 intentionally empty

    p.job.channel_map[0] = {0, tcp::SourceChannel::R};
    p.job.channel_map[1] = {1, tcp::SourceChannel::Luminance};
    p.job.channel_map[2] = {2, tcp::SourceChannel::R};
    p.job.channel_map[3] = {};

    p.job.resize_mode = tcp::ResizeMode::Custom;
    p.job.resize_filter = tcp::ResizeFilter::Lanczos;
    p.job.custom_size = {2048, 1024};
    p.job.output_format = tcp::PixelFormat::U16;
    p.output.format = tcp::exporter::Format::DDS;
    p.output.flip_vertical = true;
    return p;
}

} // namespace

TEST_CASE("project save -> load preserves every field", "[project][roundtrip]")
{
    const auto original = make_sample_project();
    const auto path = tcp::test::scratch_path("project_roundtrip.tcpproj");
    std::filesystem::remove(path);

    auto save_res = tcp::project::save(original, path);
    INFO(save_res.error);
    REQUIRE(save_res.ok);
    REQUIRE(std::filesystem::exists(path));

    auto load_res = tcp::project::load(path);
    INFO(load_res.error);
    REQUIRE(load_res.ok());

    const auto& loaded = *load_res.project;

    for (int i = 0; i < tcp::slot_count; ++i) {
        REQUIRE(loaded.job.inputs[i].path == original.job.inputs[i].path);
        REQUIRE(loaded.job.inputs[i].treat_as_srgb == original.job.inputs[i].treat_as_srgb);
        REQUIRE(loaded.job.inputs[i].image == nullptr); // loader does not re-decode pixels
    }
    for (int i = 0; i < tcp::output_channel_count; ++i) {
        REQUIRE(loaded.job.channel_map[i] == original.job.channel_map[i]);
    }
    REQUIRE(loaded.job.resize_mode == original.job.resize_mode);
    REQUIRE(loaded.job.resize_filter == original.job.resize_filter);
    REQUIRE(loaded.job.custom_size.width == original.job.custom_size.width);
    REQUIRE(loaded.job.custom_size.height == original.job.custom_size.height);
    REQUIRE(loaded.job.output_format == original.job.output_format);
    REQUIRE(loaded.output.format == original.output.format);
    REQUIRE(loaded.output.flip_vertical == original.output.flip_vertical);
}

TEST_CASE("project loader rejects an unknown schema_version", "[project][error]")
{
    const auto path = tcp::test::scratch_path("project_bad_schema.tcpproj");
    {
        std::ofstream out(path);
        out << "{\"schema_version\": 99}";
    }
    auto load_res = tcp::project::load(path);
    REQUIRE_FALSE(load_res.ok());
    REQUIRE(load_res.error.find("schema_version") != std::string::npos);
}

TEST_CASE("project loader produces a usable default for malformed inputs",
          "[project][error]")
{
    const auto path = tcp::test::scratch_path("project_bad_json.tcpproj");
    {
        std::ofstream out(path);
        out << "{this is not json}";
    }
    auto load_res = tcp::project::load(path);
    REQUIRE_FALSE(load_res.ok());
    REQUIRE_FALSE(load_res.error.empty());
}
