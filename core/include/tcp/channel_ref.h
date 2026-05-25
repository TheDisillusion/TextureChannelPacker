#pragma once

#include <cstdint>
#include <string_view>

namespace tcp {

enum class SourceChannel : std::uint8_t {
    R = 0,
    G = 1,
    B = 2,
    A = 3,
    Luminance,
};

constexpr std::string_view to_string(SourceChannel ch) noexcept
{
    switch (ch) {
        case SourceChannel::R: return "R";
        case SourceChannel::G: return "G";
        case SourceChannel::B: return "B";
        case SourceChannel::A: return "A";
        case SourceChannel::Luminance: return "L";
    }
    return "?";
}

// Reference to one source channel inside one input slot.
//
// `slot_index < 0` means the destination channel should be filled with the
// default value (see pack_job.h for what "default" means per destination).
struct ChannelRef
{
    int slot_index = -1;
    SourceChannel source = SourceChannel::R;

    [[nodiscard]] constexpr bool is_set() const noexcept { return slot_index >= 0; }
};

constexpr bool operator==(const ChannelRef& a, const ChannelRef& b) noexcept
{
    return a.slot_index == b.slot_index && a.source == b.source;
}

constexpr bool operator!=(const ChannelRef& a, const ChannelRef& b) noexcept
{
    return !(a == b);
}

} // namespace tcp
