#include "tcp/pack_job.h"

#include "tcp/resize.h"

#include <algorithm>
#include <cstring>

namespace tcp {

namespace {

float extract_channel(const Image& img, int x, int y, SourceChannel ch) noexcept
{
    if (ch == SourceChannel::Luminance) {
        // Rec.709 luminance from the first three channels; missing channels
        // contribute 0. For a 1-channel image, this is equivalent to a direct
        // read of channel 0.
        const float r = img.sample_linear(x, y, 0);
        const float g = (img.channels() > 1) ? img.sample_linear(x, y, 1) : r;
        const float b = (img.channels() > 2) ? img.sample_linear(x, y, 2) : r;
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }
    const int channel_index = static_cast<int>(ch);
    if (channel_index >= img.channels()) {
        // Asking for a channel the source doesn't have: alpha defaults to 1,
        // missing color channels default to 0.
        return (ch == SourceChannel::A) ? 1.0f : 0.0f;
    }
    return img.sample_linear(x, y, channel_index);
}

float default_for_destination(int destination_channel) noexcept
{
    return (destination_channel == 3) ? 1.0f : 0.0f;
}

} // namespace

std::optional<ImageSize> compute_target_size(const PackJob& job)
{
    if (job.resize_mode == ResizeMode::Custom) {
        if (!job.custom_size.valid()) {
            return std::nullopt;
        }
        return job.custom_size;
    }

    std::optional<ImageSize> picked;
    for (int i = 0; i < slot_count; ++i) {
        const InputSlot& s = job.inputs[i];
        if (!s.populated()) {
            continue;
        }
        const ImageSize candidate{s.image->width(), s.image->height()};
        if (!picked.has_value()) {
            picked = candidate;
            if (job.resize_mode == ResizeMode::FirstPopulated) {
                return picked;
            }
            continue;
        }
        switch (job.resize_mode) {
            case ResizeMode::Largest:
                if (candidate.width * candidate.height > picked->width * picked->height) {
                    picked = candidate;
                }
                break;
            case ResizeMode::Smallest:
                if (candidate.width * candidate.height < picked->width * picked->height) {
                    picked = candidate;
                }
                break;
            case ResizeMode::FirstPopulated:
            case ResizeMode::Custom:
                break;
        }
    }
    return picked;
}

PackResult pack(const PackJob& job)
{
    PackResult result;

    const std::optional<ImageSize> maybe_size = compute_target_size(job);
    if (!maybe_size) {
        result.error = "no populated slots and resize_mode is not Custom";
        return result;
    }
    const ImageSize target = *maybe_size;
    if (!target.valid()) {
        result.error = "target resolution is invalid";
        return result;
    }

    // Resize each populated slot to the target. Slots already at the right
    // size pay only a memcpy thanks to resize()'s fast path.
    std::array<Image, slot_count> resized;
    std::array<bool, slot_count> resized_ok{};
    for (int i = 0; i < slot_count; ++i) {
        const InputSlot& s = job.inputs[i];
        if (!s.populated()) {
            continue;
        }
        ResizeResult rr = resize(*s.image, target.width, target.height, job.resize_filter);
        if (!rr.ok()) {
            result.error = "slot " + std::to_string(i) + ": " + rr.error;
            return result;
        }
        resized[i] = std::move(rr.image);
        resized_ok[i] = true;
    }

    Image output(target.width, target.height, output_channel_count, job.output_format);

    for (int y = 0; y < target.height; ++y) {
        for (int x = 0; x < target.width; ++x) {
            for (int dst = 0; dst < output_channel_count; ++dst) {
                const ChannelRef& ref = job.channel_map[dst];
                float value;
                if (!ref.is_set() || ref.slot_index < 0 || ref.slot_index >= slot_count
                    || !resized_ok[ref.slot_index]) {
                    value = default_for_destination(dst);
                } else {
                    value = extract_channel(resized[ref.slot_index], x, y, ref.source);
                }
                output.set_linear(x, y, dst, value);
            }
        }
    }

    result.image = std::move(output);
    return result;
}

} // namespace tcp
