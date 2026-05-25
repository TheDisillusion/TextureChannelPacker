#include "tcp/channel_ref.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("default ChannelRef is unset", "[channel_ref]")
{
    constexpr tcp::ChannelRef ref{};
    STATIC_REQUIRE_FALSE(ref.is_set());
    STATIC_REQUIRE(ref.slot_index == -1);
}

TEST_CASE("ChannelRef equality matches both fields", "[channel_ref]")
{
    constexpr tcp::ChannelRef a{2, tcp::SourceChannel::G};
    constexpr tcp::ChannelRef b{2, tcp::SourceChannel::G};
    constexpr tcp::ChannelRef c{2, tcp::SourceChannel::B};
    constexpr tcp::ChannelRef d{1, tcp::SourceChannel::G};

    STATIC_REQUIRE(a == b);
    STATIC_REQUIRE(a != c);
    STATIC_REQUIRE(a != d);
}

TEST_CASE("SourceChannel to_string is human-readable", "[channel_ref]")
{
    REQUIRE(tcp::to_string(tcp::SourceChannel::R) == "R");
    REQUIRE(tcp::to_string(tcp::SourceChannel::G) == "G");
    REQUIRE(tcp::to_string(tcp::SourceChannel::B) == "B");
    REQUIRE(tcp::to_string(tcp::SourceChannel::A) == "A");
    REQUIRE(tcp::to_string(tcp::SourceChannel::Luminance) == "L");
}
