#pragma once

#include "tcp/exporter.h"
#include "tcp/pack_job.h"

#include <filesystem>
#include <optional>
#include <string>

namespace tcp::project {

// JSON schema version; bumped on any backwards-incompatible change to the
// on-disk file format. Loader rejects unknown versions.
inline constexpr int schema_version = 1;

// Extra UI-side settings that aren't part of PackJob but make a project file
// useful as a full session record.
struct OutputSettings
{
    exporter::Format format = exporter::Format::PNG;

    // Mirrors SaveOptions::flip_vertical — persisted per project so a Unity
    // user's flipped-DDS toggle survives a save/reload. Only meaningful when
    // format == DDS; the exporter ignores it for PNG / TGA.
    bool flip_vertical = false;
};

struct Project
{
    PackJob job;
    OutputSettings output;
};

struct SaveOptions
{
    bool pretty = true;
};

struct SaveResult
{
    bool ok = false;
    std::string error;
};

struct LoadResult
{
    std::optional<Project> project;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return project.has_value() && error.empty(); }
};

// Write the project to disk as JSON. Pixel data is NOT stored; only the slot
// paths and routing/output configuration. Loading will re-decode from disk.
SaveResult save(const Project& project, const std::filesystem::path& path,
                const SaveOptions& opts = {});

// Read a project file. The returned PackJob has empty image pointers for each
// populated slot — the caller is expected to re-trigger loads for the paths.
LoadResult load(const std::filesystem::path& path);

} // namespace tcp::project
