// Suppress the C++20 deprecation warning for std::filesystem::u8path; the
// replacement (constructing a path from std::u8string) requires reinterpreting
// the JSON-decoded std::string anyway, so the u8path call site is the cleanest
// way to consume a UTF-8 string here.
#define _SILENCE_CXX20_U8PATH_DEPRECATION_WARNING 1

#include "tcp/project.h"

#include "tcp/channel_ref.h"
#include "tcp/exporter.h"
#include "tcp/pack_job.h"
#include "tcp/pixel_format.h"
#include "tcp/resize.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <string_view>

namespace tcp::project {

namespace {

using nlohmann::json;

// ---- enum <-> string helpers --------------------------------------------------
//
// Stored as strings (not integer values) so the JSON survives reordering of
// enum entries between releases.

std::string_view name_of(PixelFormat f) noexcept
{
    switch (f) {
        case PixelFormat::U8: return "U8";
        case PixelFormat::U16: return "U16";
        case PixelFormat::F32: return "F32";
    }
    return "U8";
}

std::optional<PixelFormat> pixel_format_from_string(std::string_view s) noexcept
{
    if (s == "U8") return PixelFormat::U8;
    if (s == "U16") return PixelFormat::U16;
    if (s == "F32") return PixelFormat::F32;
    return std::nullopt;
}

std::string_view name_of(SourceChannel c) noexcept
{
    switch (c) {
        case SourceChannel::R: return "R";
        case SourceChannel::G: return "G";
        case SourceChannel::B: return "B";
        case SourceChannel::A: return "A";
        case SourceChannel::Luminance: return "Luminance";
    }
    return "R";
}

std::optional<SourceChannel> source_channel_from_string(std::string_view s) noexcept
{
    if (s == "R") return SourceChannel::R;
    if (s == "G") return SourceChannel::G;
    if (s == "B") return SourceChannel::B;
    if (s == "A") return SourceChannel::A;
    if (s == "Luminance" || s == "L") return SourceChannel::Luminance;
    return std::nullopt;
}

std::string_view name_of(ResizeMode m) noexcept
{
    switch (m) {
        case ResizeMode::Largest: return "Largest";
        case ResizeMode::Smallest: return "Smallest";
        case ResizeMode::FirstPopulated: return "FirstPopulated";
        case ResizeMode::Custom: return "Custom";
    }
    return "Largest";
}

std::optional<ResizeMode> resize_mode_from_string(std::string_view s) noexcept
{
    if (s == "Largest") return ResizeMode::Largest;
    if (s == "Smallest") return ResizeMode::Smallest;
    if (s == "FirstPopulated") return ResizeMode::FirstPopulated;
    if (s == "Custom") return ResizeMode::Custom;
    return std::nullopt;
}

std::string_view name_of(ResizeFilter f) noexcept
{
    switch (f) {
        case ResizeFilter::Nearest: return "Nearest";
        case ResizeFilter::Bilinear: return "Bilinear";
        case ResizeFilter::Mitchell: return "Mitchell";
        case ResizeFilter::Lanczos: return "Lanczos";
    }
    return "Mitchell";
}

std::optional<ResizeFilter> resize_filter_from_string(std::string_view s) noexcept
{
    if (s == "Nearest") return ResizeFilter::Nearest;
    if (s == "Bilinear") return ResizeFilter::Bilinear;
    if (s == "Mitchell") return ResizeFilter::Mitchell;
    if (s == "Lanczos") return ResizeFilter::Lanczos;
    return std::nullopt;
}

std::string_view name_of(exporter::Format f) noexcept
{
    switch (f) {
        case exporter::Format::PNG: return "PNG";
        case exporter::Format::TGA: return "TGA";
    }
    return "PNG";
}

std::optional<exporter::Format> exporter_format_from_string(std::string_view s) noexcept
{
    if (s == "PNG") return exporter::Format::PNG;
    if (s == "TGA") return exporter::Format::TGA;
    return std::nullopt;
}

// ---- (de)serialization --------------------------------------------------------

json input_to_json(const InputSlot& slot)
{
    if (!slot.populated() && slot.path.empty()) {
        return json::value_t::null;
    }
    return json{
        {"path", slot.path.u8string()},
        {"treat_as_srgb", slot.treat_as_srgb},
    };
}

json channel_ref_to_json(const ChannelRef& ref)
{
    if (!ref.is_set()) {
        return json::value_t::null;
    }
    return json{
        {"slot_index", ref.slot_index},
        {"source", name_of(ref.source)},
    };
}

bool input_from_json(const json& j, InputSlot& slot, std::string& error)
{
    if (j.is_null()) {
        slot = InputSlot{};
        return true;
    }
    if (!j.is_object()) {
        error = "input slot must be null or an object";
        return false;
    }
    if (j.contains("path") && j.at("path").is_string()) {
        const std::string s = j.at("path").get<std::string>();
        slot.path = std::filesystem::u8path(s);
    } else {
        slot.path.clear();
    }
    slot.treat_as_srgb = j.value("treat_as_srgb", false);
    slot.image.reset();
    return true;
}

bool channel_ref_from_json(const json& j, ChannelRef& ref, std::string& error)
{
    if (j.is_null()) {
        ref = ChannelRef{};
        return true;
    }
    if (!j.is_object()) {
        error = "channel_map entry must be null or an object";
        return false;
    }
    ref.slot_index = j.value("slot_index", -1);
    const auto src = source_channel_from_string(j.value("source", "R"));
    if (!src.has_value()) {
        error = "unknown source channel: " + j.value("source", "");
        return false;
    }
    ref.source = *src;
    return true;
}

} // namespace

SaveResult save(const Project& project, const std::filesystem::path& path,
                const SaveOptions& opts)
{
    SaveResult result;
    try {
        json root;
        root["schema_version"] = schema_version;

        json inputs = json::array();
        for (const auto& s : project.job.inputs) {
            inputs.push_back(input_to_json(s));
        }
        root["inputs"] = std::move(inputs);

        json channel_map = json::array();
        for (const auto& r : project.job.channel_map) {
            channel_map.push_back(channel_ref_to_json(r));
        }
        root["channel_map"] = std::move(channel_map);

        root["resize_mode"] = name_of(project.job.resize_mode);
        root["resize_filter"] = name_of(project.job.resize_filter);
        root["custom_size"] = {
            {"width", project.job.custom_size.width},
            {"height", project.job.custom_size.height},
        };
        root["output_bit_depth"] = name_of(project.job.output_format);
        root["output_format"] = name_of(project.output.format);

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            result.error = "could not open " + path.string() + " for writing";
            return result;
        }
        if (opts.pretty) {
            out << root.dump(2);
        } else {
            out << root.dump();
        }
        if (!out.good()) {
            result.error = "write failed for " + path.string();
            return result;
        }
        result.ok = true;
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    return result;
}

LoadResult load(const std::filesystem::path& path)
{
    LoadResult result;
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) {
            result.error = "could not open " + path.string();
            return result;
        }
        json root = json::parse(in);

        const int version = root.value("schema_version", 0);
        if (version != schema_version) {
            result.error = "unsupported schema_version " + std::to_string(version)
                           + " (expected " + std::to_string(schema_version) + ")";
            return result;
        }

        Project p;

        if (root.contains("inputs") && root.at("inputs").is_array()) {
            const auto& arr = root.at("inputs");
            const std::size_t n = std::min<std::size_t>(arr.size(), slot_count);
            for (std::size_t i = 0; i < n; ++i) {
                std::string err;
                if (!input_from_json(arr.at(i), p.job.inputs[i], err)) {
                    result.error = "inputs[" + std::to_string(i) + "]: " + err;
                    return result;
                }
            }
        }

        if (root.contains("channel_map") && root.at("channel_map").is_array()) {
            const auto& arr = root.at("channel_map");
            const std::size_t n = std::min<std::size_t>(arr.size(), output_channel_count);
            for (std::size_t i = 0; i < n; ++i) {
                std::string err;
                if (!channel_ref_from_json(arr.at(i), p.job.channel_map[i], err)) {
                    result.error = "channel_map[" + std::to_string(i) + "]: " + err;
                    return result;
                }
            }
        }

        if (const auto m = resize_mode_from_string(root.value("resize_mode", "Largest"))) {
            p.job.resize_mode = *m;
        }
        if (const auto f = resize_filter_from_string(root.value("resize_filter", "Mitchell"))) {
            p.job.resize_filter = *f;
        }
        if (root.contains("custom_size") && root.at("custom_size").is_object()) {
            const auto& cs = root.at("custom_size");
            p.job.custom_size.width = cs.value("width", 1024);
            p.job.custom_size.height = cs.value("height", 1024);
        }
        if (const auto pf = pixel_format_from_string(root.value("output_bit_depth", "U8"))) {
            p.job.output_format = *pf;
        }
        if (const auto ef = exporter_format_from_string(root.value("output_format", "PNG"))) {
            p.output.format = *ef;
        }

        result.project = std::move(p);
    } catch (const std::exception& e) {
        result.error = e.what();
    }
    return result;
}

} // namespace tcp::project
