#include "tcp/channel_ref.h"
#include "tcp/pack_job.h"
#include "tcp/preset.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

TEST_CASE("there is at least one built-in preset", "[preset]")
{
    REQUIRE_FALSE(tcp::preset::builtin_presets().empty());
}

TEST_CASE("Unreal ORM is one of the built-in presets", "[preset][unreal]")
{
    auto orm = tcp::preset::find_preset("unreal-orm");
    REQUIRE(orm.has_value());
    REQUIRE(orm->id == "unreal-orm");
    REQUIRE_FALSE(orm->display_name.empty());
    // ORM routes AO -> R, Roughness -> G, Metallic -> B.
    REQUIRE(orm->channel_map[0].slot_index == 0);
    REQUIRE(orm->channel_map[1].slot_index == 1);
    REQUIRE(orm->channel_map[2].slot_index == 2);
    REQUIRE_FALSE(orm->channel_map[3].is_set());
}

TEST_CASE("find_preset is case-insensitive", "[preset]")
{
    REQUIRE(tcp::preset::find_preset("Unreal-ORM").has_value());
    REQUIRE(tcp::preset::find_preset("UNREAL-ORM").has_value());
    REQUIRE_FALSE(tcp::preset::find_preset("does-not-exist").has_value());
}

TEST_CASE("apply_preset overwrites routing but leaves slot paths alone",
          "[preset][apply]")
{
    tcp::PackJob job;
    job.inputs[0].path = "/keep/me.png";
    job.inputs[0].treat_as_srgb = true;
    job.channel_map[0] = {3, tcp::SourceChannel::A};

    auto orm = tcp::preset::find_preset("unreal-orm");
    REQUIRE(orm.has_value());
    tcp::preset::apply_preset(job, *orm);

    // Inputs preserved.
    REQUIRE(job.inputs[0].path == std::filesystem::path("/keep/me.png"));
    REQUIRE(job.inputs[0].treat_as_srgb == true);
    // Channel map replaced.
    REQUIRE(job.channel_map[0].slot_index == 0);
    REQUIRE(job.channel_map[0].source == tcp::SourceChannel::R);
}

TEST_CASE("every built-in preset has a unique slug", "[preset]")
{
    std::set<std::string> seen;
    for (const auto& p : tcp::preset::builtin_presets()) {
        REQUIRE(seen.insert(p.id).second);
    }
}
