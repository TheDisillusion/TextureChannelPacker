#include "tcp/preset.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace tcp::preset {

namespace {

std::string lower(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

Preset make_unreal_orm()
{
    Preset p;
    p.id = "unreal-orm";
    p.display_name = "Unreal ORM";
    p.description
        = "Occlusion (R), Roughness (G), Metallic (B). The standard packed map "
          "consumed by Unreal's material editor.";
    p.slot_labels = {"AO", "Roughness", "Metallic", ""};
    p.channel_map = {
        ChannelRef{0, SourceChannel::R},
        ChannelRef{1, SourceChannel::R},
        ChannelRef{2, SourceChannel::R},
        ChannelRef{},
    };
    p.resize_filter = ResizeFilter::Mitchell;
    p.output_format = PixelFormat::U8;
    p.output_kind = exporter::Format::PNG;
    return p;
}

Preset make_unreal_mra()
{
    Preset p;
    p.id = "unreal-mra";
    p.display_name = "Unreal MRA";
    p.description
        = "Metallic (R), Roughness (G), AO (B). Alternate packing convention; "
          "common in legacy Unreal pipelines.";
    p.slot_labels = {"Metallic", "Roughness", "AO", ""};
    p.channel_map = {
        ChannelRef{0, SourceChannel::R},
        ChannelRef{1, SourceChannel::R},
        ChannelRef{2, SourceChannel::R},
        ChannelRef{},
    };
    p.resize_filter = ResizeFilter::Mitchell;
    return p;
}

Preset make_normal_height()
{
    Preset p;
    p.id = "normal-height";
    p.display_name = "Normal + Height";
    p.description
        = "Normal XY (R, G), Height (B), 1.0 (A). Slot 0 should be a normal map; "
          "slot 1 should be a single-channel height map.";
    p.slot_labels = {"Normal", "Height", "", ""};
    p.channel_map = {
        ChannelRef{0, SourceChannel::R},
        ChannelRef{0, SourceChannel::G},
        ChannelRef{1, SourceChannel::R},
        ChannelRef{},
    };
    p.resize_filter = ResizeFilter::Mitchell;
    p.output_format = PixelFormat::U16;
    return p;
}

Preset make_diffuse_specular_alpha()
{
    Preset p;
    p.id = "diffuse-specular-alpha";
    p.display_name = "Diffuse + Specular Alpha";
    p.description
        = "Diffuse RGB from slot 0, specular intensity packed into A from slot 1's "
          "luminance. Common in older rendering pipelines.";
    p.slot_labels = {"Diffuse", "Specular", "", ""};
    p.channel_map = {
        ChannelRef{0, SourceChannel::R},
        ChannelRef{0, SourceChannel::G},
        ChannelRef{0, SourceChannel::B},
        ChannelRef{1, SourceChannel::Luminance},
    };
    p.resize_filter = ResizeFilter::Mitchell;
    return p;
}

} // namespace

const std::vector<Preset>& builtin_presets()
{
    static const std::vector<Preset> presets = {
        make_unreal_orm(),
        make_unreal_mra(),
        make_normal_height(),
        make_diffuse_specular_alpha(),
    };
    return presets;
}

std::optional<Preset> find_preset(std::string_view id)
{
    const std::string needle = lower(id);
    for (const auto& p : builtin_presets()) {
        if (lower(p.id) == needle) {
            return p;
        }
    }
    return std::nullopt;
}

void apply_preset(PackJob& job, const Preset& preset)
{
    job.channel_map = preset.channel_map;
    job.resize_mode = preset.resize_mode;
    job.resize_filter = preset.resize_filter;
    job.output_format = preset.output_format;
    // Inputs (paths and pixel data) intentionally left untouched.
}

} // namespace tcp::preset
