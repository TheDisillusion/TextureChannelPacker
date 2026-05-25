// ----------------------------------------------------------------------------
// tcp — headless driver for the Texture Channel Packer core library.
//
// Subcommands:
//   list-presets                     Print the built-in routing presets.
//   pack <project.tcpproj>           Pack the inputs listed in a .tcpproj file.
//   pack --preset <id> --slot N=path Pack a one-off using a built-in preset.
//
// Examples:
//   tcp list-presets
//   tcp pack project.tcpproj -o out.png
//   tcp pack --preset unreal-orm \
//            --slot 0=ao.png --slot 1=rough.png --slot 2=metal.png \
//            -o packed.png
// ----------------------------------------------------------------------------

#include "tcp/exporter.h"
#include "tcp/loader.h"
#include "tcp/pack_job.h"
#include "tcp/preset.h"
#include "tcp/project.h"
#include "tcp/version.h"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace fs = std::filesystem;

// Returns 0 on success, non-zero on error. Errors are printed to stderr.
constexpr int exit_ok = 0;
constexpr int exit_user_error = 2;
constexpr int exit_io_error = 3;
constexpr int exit_runtime_error = 4;

void print_preset_line(const tcp::preset::Preset& p)
{
    std::cout << "  " << p.id << "  (" << p.display_name << ")\n";
    std::cout << "      " << p.description << "\n";
    std::cout << "      slots:";
    for (int i = 0; i < tcp::slot_count; ++i) {
        const auto& label = p.slot_labels[i];
        if (label.empty()) {
            continue;
        }
        std::cout << " " << i << "=" << label;
    }
    std::cout << "\n";
}

int cmd_list_presets()
{
    const auto& presets = tcp::preset::builtin_presets();
    std::cout << "Built-in presets (" << presets.size() << "):\n";
    for (const auto& p : presets) {
        print_preset_line(p);
    }
    return exit_ok;
}

// Parse "N=path" into (slot_index, path). Returns false on malformed input.
bool parse_slot_assignment(const std::string& s, int& slot_out, std::string& path_out)
{
    const auto eq = s.find('=');
    if (eq == std::string::npos || eq == 0) {
        return false;
    }
    try {
        slot_out = std::stoi(s.substr(0, eq));
    } catch (...) {
        return false;
    }
    if (slot_out < 0 || slot_out >= tcp::slot_count) {
        return false;
    }
    path_out = s.substr(eq + 1);
    return !path_out.empty();
}

// Load each populated slot into the PackJob. Returns the index of the first
// failing slot, or -1 on full success.
int load_inputs(tcp::PackJob& job, std::string& error_out)
{
    for (int i = 0; i < tcp::slot_count; ++i) {
        auto& slot = job.inputs[i];
        if (slot.path.empty()) {
            continue;
        }
        tcp::loader::LoadOptions opts;
        opts.convert_srgb_to_linear = slot.treat_as_srgb;
        auto res = tcp::loader::load(slot.path, opts);
        if (!res.ok()) {
            error_out = "slot " + std::to_string(i) + " (" + slot.path.string() + "): " + res.error;
            return i;
        }
        slot.image = res.image;
    }
    return -1;
}

fs::path infer_output_extension(tcp::exporter::Format kind)
{
    return (kind == tcp::exporter::Format::PNG) ? fs::path("packed.png") : fs::path("packed.tga");
}

int cmd_pack_project(const fs::path& project_path, const std::optional<fs::path>& output_override,
                     bool quiet)
{
    auto load_res = tcp::project::load(project_path);
    if (!load_res.ok()) {
        std::cerr << "error: " << load_res.error << "\n";
        return exit_user_error;
    }
    tcp::project::Project& project = *load_res.project;

    std::string err;
    if (load_inputs(project.job, err) >= 0) {
        std::cerr << "error: " << err << "\n";
        return exit_io_error;
    }

    auto packed = tcp::pack(project.job);
    if (!packed.ok()) {
        std::cerr << "error: pack failed: " << packed.error << "\n";
        return exit_runtime_error;
    }

    const fs::path out_path = output_override
                                  ? *output_override
                                  : project_path.parent_path() / infer_output_extension(project.output.format);

    tcp::exporter::SaveOptions save_opts;
    save_opts.format = project.output.format;
    auto save_res = tcp::exporter::save(*packed.image, out_path, save_opts);
    if (!save_res.ok) {
        std::cerr << "error: save failed: " << save_res.error << "\n";
        return exit_io_error;
    }

    if (!quiet) {
        std::cout << "wrote " << out_path.string() << "\n";
    }
    return exit_ok;
}

int cmd_pack_preset(const std::string& preset_id,
                    const std::vector<std::string>& slot_assignments,
                    const std::optional<fs::path>& output_override, bool quiet)
{
    auto p = tcp::preset::find_preset(preset_id);
    if (!p) {
        std::cerr << "error: unknown preset '" << preset_id
                  << "'. Run 'tcp list-presets' to see what's available.\n";
        return exit_user_error;
    }

    tcp::PackJob job;
    tcp::preset::apply_preset(job, *p);

    for (const auto& s : slot_assignments) {
        int slot_idx = 0;
        std::string path_str;
        if (!parse_slot_assignment(s, slot_idx, path_str)) {
            std::cerr << "error: bad --slot value '" << s
                      << "'. Expected N=PATH where N is 0..3.\n";
            return exit_user_error;
        }
        job.inputs[slot_idx].path = fs::path(path_str);
    }

    if (!std::any_of(job.inputs.begin(), job.inputs.end(),
                     [](const tcp::InputSlot& s) { return !s.path.empty(); })) {
        std::cerr << "error: at least one --slot N=PATH is required.\n";
        return exit_user_error;
    }

    std::string err;
    if (load_inputs(job, err) >= 0) {
        std::cerr << "error: " << err << "\n";
        return exit_io_error;
    }

    auto packed = tcp::pack(job);
    if (!packed.ok()) {
        std::cerr << "error: pack failed: " << packed.error << "\n";
        return exit_runtime_error;
    }

    const fs::path out_path = output_override
                                  ? *output_override
                                  : fs::current_path() / infer_output_extension(p->output_kind);

    tcp::exporter::SaveOptions save_opts;
    save_opts.format = p->output_kind;
    auto save_res = tcp::exporter::save(*packed.image, out_path, save_opts);
    if (!save_res.ok) {
        std::cerr << "error: save failed: " << save_res.error << "\n";
        return exit_io_error;
    }

    if (!quiet) {
        std::cout << "wrote " << out_path.string() << " (preset: " << p->id << ")\n";
    }
    return exit_ok;
}

} // namespace

int main(int argc, char** argv)
{
    CLI::App app{"Texture Channel Packer — pack R/G/B/A channels of texture maps."};
    app.set_version_flag("--version", std::string(tcp::version_string()));
    app.require_subcommand(1);

    bool quiet = false;
    app.add_flag("-q,--quiet", quiet, "Suppress success messages on stdout.");

    auto* list = app.add_subcommand("list-presets", "Print the built-in routing presets.");

    auto* pack = app.add_subcommand("pack", "Pack inputs into one output texture.");
    std::string project_path_arg;
    std::string preset_id_arg;
    std::vector<std::string> slot_assignments;
    std::string output_path_arg;
    pack->add_option("project", project_path_arg,
                     "Path to a .tcpproj file. Either this or --preset is required.");
    pack->add_option("--preset", preset_id_arg,
                     "Use a built-in preset by id (run 'tcp list-presets' for choices).");
    pack->add_option("--slot", slot_assignments,
                     "Assign an input slot in the form N=PATH (e.g. --slot 0=ao.png). "
                     "Required when using --preset. Repeatable.")
        ->take_all();
    pack->add_option("-o,--output", output_path_arg,
                     "Output texture path. Defaults to packed.png/.tga next to the project.");

    CLI11_PARSE(app, argc, argv);

    if (list->parsed()) {
        return cmd_list_presets();
    }
    if (pack->parsed()) {
        std::optional<std::filesystem::path> output_override;
        if (!output_path_arg.empty()) {
            output_override = std::filesystem::path(output_path_arg);
        }

        const bool have_project = !project_path_arg.empty();
        const bool have_preset = !preset_id_arg.empty();
        if (have_project == have_preset) {
            std::cerr << "error: pack requires exactly one of <project> or --preset.\n";
            return exit_user_error;
        }

        if (have_project) {
            return cmd_pack_project(std::filesystem::path(project_path_arg), output_override, quiet);
        }
        return cmd_pack_preset(preset_id_arg, slot_assignments, output_override, quiet);
    }

    return exit_ok;
}
