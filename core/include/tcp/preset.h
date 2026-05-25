#pragma once

#include "tcp/channel_ref.h"
#include "tcp/exporter.h"
#include "tcp/pack_job.h"
#include "tcp/pixel_format.h"
#include "tcp/resize.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tcp::preset {

// A packing template. Defines the routing for a workflow (e.g. "Unreal ORM")
// without committing to any specific input files. Apply with apply_preset()
// to a PackJob that already has its inputs populated.
struct Preset
{
    std::string id;             // machine-friendly slug, e.g. "unreal-orm"
    std::string display_name;   // shown to humans, e.g. "Unreal ORM"
    std::string description;

    // Human-readable label per slot ("AO", "Roughness", "Metallic", "").
    // Empty means the slot is unused by this preset.
    std::array<std::string, slot_count> slot_labels{};

    // The routing this preset enforces.
    std::array<ChannelRef, output_channel_count> channel_map{};

    // Sensible defaults for the rest of the job.
    ResizeMode resize_mode = ResizeMode::Largest;
    ResizeFilter resize_filter = ResizeFilter::Mitchell;
    PixelFormat output_format = PixelFormat::U8;
    exporter::Format output_kind = exporter::Format::PNG;
};

// All presets that ship with the binary.
[[nodiscard]] const std::vector<Preset>& builtin_presets();

// Look up a preset by its id. Case-insensitive match on the slug.
[[nodiscard]] std::optional<Preset> find_preset(std::string_view id);

// Copy a preset's routing + defaults onto a PackJob. Input slot images and
// paths are not touched — the caller is expected to drop or load files into
// the slots whose labels indicate they should be populated.
void apply_preset(PackJob& job, const Preset& preset);

} // namespace tcp::preset
