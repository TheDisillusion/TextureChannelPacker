#pragma once

#include "tcp/channel_ref.h"
#include "tcp/exporter.h"
#include "tcp/pack_job.h"
#include "tcp/pixel_format.h"
#include "tcp/project.h"
#include "tcp/resize.h"

#include <QObject>
#include <QString>

#include <memory>

namespace tcp {
class Image;
}

namespace tcp::app {

// Sole owner of the active PackJob. Mutations from the UI funnel through this
// object's slots; readers subscribe to its signals. All heavy work (load,
// export) is dispatched to QThreadPool; the controller marshals results back
// to the GUI thread before emitting "loaded" / "exportFinished".
class JobController : public QObject
{
    Q_OBJECT

public:
    explicit JobController(QObject* parent = nullptr);

    [[nodiscard]] const PackJob& job() const noexcept { return job_; }
    [[nodiscard]] bool any_slot_populated() const noexcept;
    [[nodiscard]] exporter::Format output_format_kind() const noexcept { return output_format_kind_; }

    // Drop everything and start from scratch (used by File > New).
    void reset();

    // Apply a freshly-loaded project: copy the settings into the active job,
    // wipe any prior image data, and trigger async loads for each populated
    // path. The corresponding signals fire as if the user dropped each file.
    void apply_project(const project::Project& project);

    // Take a snapshot of the current state for serialization.
    [[nodiscard]] project::Project snapshot_project() const;

    // Async slot mutations. Each returns immediately; subscribe to signals.
    void load_slot(int slot_index, const QString& path);
    void clear_slot(int slot_index);
    void set_slot_source_channel(int slot_index, SourceChannel ch);
    void set_slot_treat_as_srgb(int slot_index, bool srgb);

    void set_channel_map(int destination_index, ChannelRef ref);

    void set_resize_mode(ResizeMode mode);
    void set_resize_filter(ResizeFilter filter);
    void set_custom_size(int width, int height);
    void set_output_format(PixelFormat fmt);
    void set_output_format_kind(exporter::Format fmt);

    // Synchronous: the underlying pack op is fast enough that off-thread isn't
    // necessary for export. Saving to disk IS off-thread.
    void export_to(const QString& path, exporter::Format format);

signals:
    void slot_loading(int slot_index, const QString& path);
    void slot_loaded(int slot_index);
    void slot_load_failed(int slot_index, const QString& error);
    void slot_cleared(int slot_index);

    void slot_source_channel_changed(int slot_index, tcp::SourceChannel channel);
    void slot_treat_as_srgb_changed(int slot_index, bool srgb);

    void channel_map_changed(int destination_index, tcp::ChannelRef ref);

    void resize_mode_changed(tcp::ResizeMode mode);
    void resize_filter_changed(tcp::ResizeFilter filter);
    void custom_size_changed(int width, int height);
    void output_format_changed(tcp::PixelFormat fmt);
    void output_format_kind_changed(tcp::exporter::Format fmt);

    // Fires after apply_project replaces the whole job state; widgets should
    // re-sync from controller->job() in response.
    void project_reset();

    void target_size_changed(int width, int height); // -1, -1 when unset
    void populated_slots_changed(int populated);

    void export_started(const QString& path);
    void export_finished(const QString& path);
    void export_failed(const QString& path, const QString& error);

private:
    void emit_derived_signals_();
    void install_loaded_image_(int slot_index,
                               std::shared_ptr<const tcp::Image> image,
                               const QString& path);

    PackJob job_;
    exporter::Format output_format_kind_ = exporter::Format::PNG;
};

} // namespace tcp::app
