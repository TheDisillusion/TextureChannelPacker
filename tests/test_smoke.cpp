#include "tcp/version.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("version constants form a valid semver triple", "[smoke][version]")
{
    REQUIRE(tcp::version_major >= 0);
    REQUIRE(tcp::version_minor >= 0);
    REQUIRE(tcp::version_patch >= 0);
}

TEST_CASE("version_string matches the version constants", "[smoke][version]")
{
    // Derive the expected string from the constants so a version bump only
    // needs CMakeLists.txt + version.h + version.cpp to be in sync. The test
    // doesn't need touching on every release.
    const std::string expected = std::to_string(tcp::version_major) + "."
                                 + std::to_string(tcp::version_minor) + "."
                                 + std::to_string(tcp::version_patch);
    REQUIRE_FALSE(tcp::version_string().empty());
    REQUIRE(tcp::version_string() == expected);
}
