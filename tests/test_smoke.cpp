#include "tcp/version.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("version constants are wired", "[smoke][version]")
{
    REQUIRE(tcp::version_major == 0);
    REQUIRE(tcp::version_minor == 0);
    REQUIRE(tcp::version_patch == 0);
}

TEST_CASE("version_string returns a non-empty value", "[smoke][version]")
{
    REQUIRE_FALSE(tcp::version_string().empty());
    REQUIRE(tcp::version_string() == "0.0.0");
}
