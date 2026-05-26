#include "job_controller.h"

#include "tcp/exporter.h"
#include "tcp/image.h"
#include "tcp/loader.h"
#include "tcp/pack_job.h"

#include <QPointer>
#include <QString>
#include <QThreadPool>

#include <filesystem>
#include <utility>

namespace tcp::app {

namespace {

std::filesystem::path qstring_to_path(const QString& s)
{
    return std::filesystem::path(s.toStdU16String());
}

} // namespace

JobController::JobController(QObject* parent)
    : QObject(parent)
{
    // Default channel routing: each destination pulls from the slot at the
    // matching index, using the matching source channel. This is the
    // "identity" pack and is the most common case for a fresh job (drop a
    // mask map into each slot and you get the obvious packed output).
    for (int i = 0; i < output_channel_count; ++i) {
        job_.channel_map[i] = {i, static_cast<SourceChannel>(i)};
    }
}

bool JobController::any_slot_populated() const noexcept
{
    for (const auto& s : job_.inputs) {
        if (s.populated()) {
            return true;
        }
    }
    return false;
}

void JobController::load_slot(int slot_index, const QString& path)
{
    if (slot_index < 0 || slot_index >= slot_count) {
        return;
    }

    emit slot_loading(slot_index, path);

    QPointer<JobController> self(this);
    const QString captured_path = path;
    const bool treat_as_srgb = job_.inputs[slot_index].treat_as_srgb;
    const int captured_slot = slot_index;

    QThreadPool::globalInstance()->start([self, captured_slot, captured_path, treat_as_srgb] {
        loader::LoadOptions opts;
        opts.convert_srgb_to_linear = treat_as_srgb;
        auto result = loader::load(qstring_to_path(captured_path), opts);

        QMetaObject::invokeMethod(
            self.data(),
            [self, captured_slot, captured_path, result = std::move(result)]() mutable {
                if (!self) {
                    return;
                }
                if (!result.ok()) {
                    emit self->slot_load_failed(captured_slot,
                                                QString::fromStdString(result.error));
                    return;
                }
                self->install_loaded_image_(captured_slot, result.image, captured_path);
            },
            Qt::QueuedConnection);
    });
}

void JobController::install_loaded_image_(int slot_index,
                                          std::shared_ptr<const tcp::Image> image,
                                          const QString& path)
{
    job_.inputs[slot_index].image = std::move(image);
    job_.inputs[slot_index].path = qstring_to_path(path);
    emit slot_loaded(slot_index);
    emit_derived_signals_();
}

void JobController::clear_slot(int slot_index)
{
    if (slot_index < 0 || slot_index >= slot_count) {
        return;
    }
    job_.inputs[slot_index].image.reset();
    job_.inputs[slot_index].path.clear();
    emit slot_cleared(slot_index);
    emit_derived_signals_();
}

void JobController::set_slot_source_channel(int slot_index, SourceChannel ch)
{
    if (slot_index < 0 || slot_index >= slot_count) {
        return;
    }
    // The "source channel" of a slot is encoded in the channel_map entry whose
    // slot_index matches. We update every map entry pointing at this slot.
    for (int i = 0; i < output_channel_count; ++i) {
        if (job_.channel_map[i].slot_index == slot_index) {
            job_.channel_map[i].source = ch;
            emit channel_map_changed(i, job_.channel_map[i]);
        }
    }
    emit slot_source_channel_changed(slot_index, ch);
}

void JobController::set_slot_treat_as_srgb(int slot_index, bool srgb)
{
    if (slot_index < 0 || slot_index >= slot_count) {
        return;
    }
    job_.inputs[slot_index].treat_as_srgb = srgb;
    emit slot_treat_as_srgb_changed(slot_index, srgb);
    // The current decoded image was loaded with the old setting. A real reload
    // is needed to apply the change; the UI is expected to trigger a reload.
}

void JobController::set_channel_map(int destination_index, ChannelRef ref)
{
    if (destination_index < 0 || destination_index >= output_channel_count) {
        return;
    }
    job_.channel_map[destination_index] = ref;
    emit channel_map_changed(destination_index, ref);
}

void JobController::set_resize_mode(ResizeMode mode)
{
    if (job_.resize_mode == mode) {
        return;
    }
    job_.resize_mode = mode;
    emit resize_mode_changed(mode);
    emit_derived_signals_();
}

void JobController::set_resize_filter(ResizeFilter filter)
{
    if (job_.resize_filter == filter) {
        return;
    }
    job_.resize_filter = filter;
    emit resize_filter_changed(filter);
}

void JobController::set_custom_size(int width, int height)
{
    if (job_.custom_size.width == width && job_.custom_size.height == height) {
        return;
    }
    job_.custom_size = {width, height};
    emit custom_size_changed(width, height);
    if (job_.resize_mode == ResizeMode::Custom) {
        emit_derived_signals_();
    }
}

void JobController::set_output_format(PixelFormat fmt)
{
    if (job_.output_format == fmt) {
        return;
    }
    job_.output_format = fmt;
    emit output_format_changed(fmt);
}

void JobController::set_output_format_kind(exporter::Format fmt)
{
    if (output_format_kind_ == fmt) {
        return;
    }
    output_format_kind_ = fmt;
    emit output_format_kind_changed(fmt);
}

void JobController::set_bc_variant(exporter::BcVariant variant)
{
    if (bc_variant_ == variant) {
        return;
    }
    bc_variant_ = variant;
    emit bc_variant_changed(variant);
}

void JobController::set_flip_vertical_y(bool flip)
{
    if (flip_vertical_y_ == flip) {
        return;
    }
    flip_vertical_y_ = flip;
    emit flip_vertical_y_changed(flip);
}

void JobController::reset()
{
    apply_project(project::Project{});
}

project::Project JobController::snapshot_project() const
{
    project::Project p;
    p.job = job_;
    // Strip Image pointers — the on-disk format only stores paths.
    for (auto& s : p.job.inputs) {
        s.image.reset();
    }
    p.output.format = output_format_kind_;
    p.output.flip_vertical = flip_vertical_y_;
    return p;
}

void JobController::apply_project(const project::Project& project)
{
    job_ = project.job;
    // Discard any pixel data that came in via copy — projects only ever
    // store paths on disk, but defensively wipe in case a caller passed a
    // live job by value.
    for (auto& s : job_.inputs) {
        s.image.reset();
    }
    output_format_kind_ = project.output.format;
    flip_vertical_y_ = project.output.flip_vertical;

    emit project_reset();
    emit output_format_kind_changed(output_format_kind_);
    emit flip_vertical_y_changed(flip_vertical_y_);
    emit output_format_changed(job_.output_format);
    emit resize_mode_changed(job_.resize_mode);
    emit resize_filter_changed(job_.resize_filter);
    emit custom_size_changed(job_.custom_size.width, job_.custom_size.height);
    for (int i = 0; i < output_channel_count; ++i) {
        emit channel_map_changed(i, job_.channel_map[i]);
    }
    for (int i = 0; i < slot_count; ++i) {
        emit slot_cleared(i);
    }
    emit_derived_signals_();

    // Re-trigger async loads for any populated path. Each load() fires the
    // standard slot_loading -> slot_loaded sequence so widgets repaint.
    for (int i = 0; i < slot_count; ++i) {
        const auto& path = job_.inputs[i].path;
        if (!path.empty()) {
            load_slot(i, QString::fromStdU16String(path.u16string()));
        }
    }
}

void JobController::export_to(const QString& path, exporter::Format format)
{
    emit export_started(path);

    QPointer<JobController> self(this);
    // Snapshot the job: subsequent UI edits during the export shouldn't change
    // what gets written. PackJob is a value type holding shared_ptr's, so the
    // copy is cheap.
    PackJob snapshot = job_;
    const QString captured_path = path;
    const exporter::Format captured_format = format;
    const exporter::BcVariant captured_bc = bc_variant_;
    const bool captured_flip = flip_vertical_y_;

    QThreadPool::globalInstance()->start(
        [self, snapshot = std::move(snapshot), captured_path, captured_format, captured_bc,
         captured_flip] {
            auto packed = pack(snapshot);
            QString error_text;
            if (!packed.ok()) {
                error_text = QString::fromStdString(packed.error);
            } else {
                exporter::SaveOptions opts;
                opts.format = captured_format;
                opts.bc_variant = captured_bc;
                opts.flip_vertical = captured_flip;
                auto save_res = exporter::save(*packed.image, qstring_to_path(captured_path), opts);
                if (!save_res.ok) {
                    error_text = QString::fromStdString(save_res.error);
                }
            }
            QMetaObject::invokeMethod(
                self.data(),
                [self, captured_path, error_text] {
                    if (!self) {
                        return;
                    }
                    if (error_text.isEmpty()) {
                        emit self->export_finished(captured_path);
                    } else {
                        emit self->export_failed(captured_path, error_text);
                    }
                },
                Qt::QueuedConnection);
        });
}

void JobController::emit_derived_signals_()
{
    int populated = 0;
    for (const auto& s : job_.inputs) {
        if (s.populated()) {
            ++populated;
        }
    }
    emit populated_slots_changed(populated);

    const auto target = compute_target_size(job_);
    if (target.has_value()) {
        emit target_size_changed(target->width, target->height);
    } else {
        emit target_size_changed(-1, -1);
    }
}

} // namespace tcp::app
