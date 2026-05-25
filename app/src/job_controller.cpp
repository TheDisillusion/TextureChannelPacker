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

    QThreadPool::globalInstance()->start(
        [self, snapshot = std::move(snapshot), captured_path, captured_format] {
            auto packed = pack(snapshot);
            QString error_text;
            if (!packed.ok()) {
                error_text = QString::fromStdString(packed.error);
            } else {
                exporter::SaveOptions opts;
                opts.format = captured_format;
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
