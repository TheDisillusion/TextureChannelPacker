#include "tcp/exporter.h"
#include "tcp/image.h"
#include "tcp/pixel_format.h"

#include "test_support.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace {

constexpr std::array<std::uint8_t, 4> dds_magic{'D', 'D', 'S', ' '};

bool starts_with_dds_magic(const std::filesystem::path& p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        return false;
    }
    std::array<std::uint8_t, 4> buf{};
    f.read(reinterpret_cast<char*>(buf.data()), buf.size());
    return f.gcount() == 4 && buf == dds_magic;
}

} // namespace

TEST_CASE("exporter format_from_extension recognizes .dds", "[exporter][dds]")
{
    using tcp::exporter::Format;
    using tcp::exporter::format_from_extension;
    REQUIRE(format_from_extension("foo.dds") == Format::DDS);
    REQUIRE(format_from_extension("FOO.DDS") == Format::DDS);
}

TEST_CASE("bc_variant_from_string accepts the documented names",
          "[exporter][dds][bc]")
{
    using tcp::exporter::BcVariant;
    using tcp::exporter::bc_variant_from_string;

    REQUIRE(bc_variant_from_string("uncompressed") == BcVariant::Uncompressed);
    REQUIRE(bc_variant_from_string("bc1") == BcVariant::BC1);
    REQUIRE(bc_variant_from_string("BC1") == BcVariant::BC1);
    REQUIRE(bc_variant_from_string("dxt1") == BcVariant::BC1);
    REQUIRE(bc_variant_from_string("bc3") == BcVariant::BC3);
    REQUIRE(bc_variant_from_string("dxt5") == BcVariant::BC3);
    REQUIRE(bc_variant_from_string("bc5") == BcVariant::BC5);
    REQUIRE(bc_variant_from_string("bc7") == BcVariant::BC7);
    REQUIRE_FALSE(bc_variant_from_string("bc99").has_value());
}

TEST_CASE("DDS uncompressed write produces a valid DDS file",
          "[exporter][dds]")
{
    const auto img = tcp::test::make_solid(8, 8, tcp::PixelFormat::U8,
                                           0.5f, 0.25f, 0.75f, 1.0f);
    const auto path = tcp::test::scratch_path("uncompressed.dds");
    std::filesystem::remove(path);

    tcp::exporter::SaveOptions opts;
    opts.format = tcp::exporter::Format::DDS;
    opts.bc_variant = tcp::exporter::BcVariant::Uncompressed;

    auto res = tcp::exporter::save(img, path, opts);
    INFO(res.error);
    REQUIRE(res.ok);
    REQUIRE(std::filesystem::exists(path));
    REQUIRE(std::filesystem::file_size(path) > 4);
    REQUIRE(starts_with_dds_magic(path));
}

TEST_CASE("DDS BC1 encodes a 4x4-aligned image", "[exporter][dds][bc1]")
{
    const auto img = tcp::test::make_solid(8, 8, tcp::PixelFormat::U8,
                                           1.0f, 0.0f, 0.0f, 1.0f);
    const auto path = tcp::test::scratch_path("bc1.dds");
    std::filesystem::remove(path);

    tcp::exporter::SaveOptions opts;
    opts.format = tcp::exporter::Format::DDS;
    opts.bc_variant = tcp::exporter::BcVariant::BC1;

    auto res = tcp::exporter::save(img, path, opts);
    INFO(res.error);
    REQUIRE(res.ok);
    REQUIRE(starts_with_dds_magic(path));
}

TEST_CASE("DDS BC3 and BC7 encode color + alpha", "[exporter][dds][bc][slow]")
{
    const auto img = tcp::test::make_solid(8, 8, tcp::PixelFormat::U8,
                                           0.8f, 0.2f, 0.4f, 0.5f);

    for (auto v : {tcp::exporter::BcVariant::BC3, tcp::exporter::BcVariant::BC7}) {
        tcp::exporter::SaveOptions opts;
        opts.format = tcp::exporter::Format::DDS;
        opts.bc_variant = v;

        const auto path = tcp::test::scratch_path(
            std::string("bc_") + std::string(tcp::exporter::to_string(v)) + ".dds");
        std::filesystem::remove(path);

        auto res = tcp::exporter::save(img, path, opts);
        INFO(res.error);
        REQUIRE(res.ok);
        REQUIRE(starts_with_dds_magic(path));
    }
}

TEST_CASE("DDS BC rejects non-multiple-of-4 dimensions",
          "[exporter][dds][bc][error]")
{
    const auto img = tcp::test::make_solid(5, 5, tcp::PixelFormat::U8,
                                           1.0f, 0.0f, 0.0f, 1.0f);
    const auto path = tcp::test::scratch_path("bad_size.dds");
    tcp::exporter::SaveOptions opts;
    opts.format = tcp::exporter::Format::DDS;
    opts.bc_variant = tcp::exporter::BcVariant::BC1;
    auto res = tcp::exporter::save(img, path, opts);
    INFO(res.error);
    REQUIRE_FALSE(res.ok);
    REQUIRE(res.error.find("multiples of 4") != std::string::npos);
}

// Orientation: build an image where the top row is red and the bottom row is
// blue, save uncompressed DDS twice (flip off, flip on), read the first row
// of pixel bytes from each file, and verify which source row landed on top.
//
// Uncompressed DDS layout is well-defined: 4-byte 'DDS ' magic, 124-byte
// DDS_HEADER, then raw RGBA8 pixels in scan order. Reading the first 4*width
// bytes after offset 128 gives us the top scanline as written, which lets us
// assert orientation without pulling DirectXTex into the test target.
TEST_CASE("DDS uncompressed export honors flip_vertical",
          "[exporter][dds][orientation]")
{
    constexpr int w = 8;
    constexpr int h = 8;
    constexpr std::size_t dds_header_size = 128; // 'DDS ' + DDS_HEADER

    tcp::Image img(w, h, 4, tcp::PixelFormat::U8);
    for (int y = 0; y < h; ++y) {
        const float r = (y == 0) ? 1.0f : 0.0f;
        const float b = (y == h - 1) ? 1.0f : 0.0f;
        for (int x = 0; x < w; ++x) {
            img.set_linear(x, y, 0, r);
            img.set_linear(x, y, 1, 0.0f);
            img.set_linear(x, y, 2, b);
            img.set_linear(x, y, 3, 1.0f);
        }
    }

    auto read_first_row = [](const std::filesystem::path& path) {
        std::array<std::uint8_t, w * 4> row{};
        std::ifstream f(path, std::ios::binary);
        REQUIRE(f.is_open());
        f.seekg(static_cast<std::streamoff>(dds_header_size), std::ios::beg);
        f.read(reinterpret_cast<char*>(row.data()), static_cast<std::streamsize>(row.size()));
        REQUIRE(f.gcount() == static_cast<std::streamsize>(row.size()));
        return row;
    };

    SECTION("flip_vertical = false leaves the top row at the top of the file")
    {
        const auto path = tcp::test::scratch_path("orient_noflip.dds");
        std::filesystem::remove(path);

        tcp::exporter::SaveOptions opts;
        opts.format = tcp::exporter::Format::DDS;
        opts.bc_variant = tcp::exporter::BcVariant::Uncompressed;
        opts.flip_vertical = false;
        auto res = tcp::exporter::save(img, path, opts);
        INFO(res.error);
        REQUIRE(res.ok);

        const auto row = read_first_row(path);
        // First file row should be source row 0 — R=255, B=0.
        REQUIRE(row[0] == 255); // R
        REQUIRE(row[2] == 0);   // B
    }

    SECTION("flip_vertical = true puts the last source row at the top of the file")
    {
        const auto path = tcp::test::scratch_path("orient_flip.dds");
        std::filesystem::remove(path);

        tcp::exporter::SaveOptions opts;
        opts.format = tcp::exporter::Format::DDS;
        opts.bc_variant = tcp::exporter::BcVariant::Uncompressed;
        opts.flip_vertical = true;
        auto res = tcp::exporter::save(img, path, opts);
        INFO(res.error);
        REQUIRE(res.ok);

        const auto row = read_first_row(path);
        // First file row should be source row h-1 — R=0, B=255.
        REQUIRE(row[0] == 0);   // R
        REQUIRE(row[2] == 255); // B
    }
}
