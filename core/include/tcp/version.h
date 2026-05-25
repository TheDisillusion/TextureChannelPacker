#pragma once

#include <string_view>

namespace tcp {

inline constexpr int version_major = 0;
inline constexpr int version_minor = 3;
inline constexpr int version_patch = 0;

std::string_view version_string() noexcept;

} // namespace tcp
