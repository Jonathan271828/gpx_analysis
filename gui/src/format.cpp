#include "format.hpp"

#include <cstdio>

namespace gui::fmt {

namespace {

constexpr long kSecondsPerMinute = 60;
constexpr long kSecondsPerHour   = 3600;

} // namespace

std::string compact_duration(Long seconds) {
    if (seconds < kSecondsPerMinute) return std::to_string(seconds) + "s";
    if (seconds < kSecondsPerHour)   return std::to_string(seconds / kSecondsPerMinute) + "m";
    return std::to_string(seconds / kSecondsPerHour) + "h";
}

std::string elapsed_clock(double seconds) {
    const long total = seconds > 0.0 ? static_cast<long>(seconds + 0.5) : 0;
    const long h = total / kSecondsPerHour;
    const long m = (total % kSecondsPerHour) / kSecondsPerMinute;
    const long s = total % kSecondsPerMinute;

    char buf[32];
    if (h > 0) std::snprintf(buf, sizeof buf, "%ld:%02ld:%02ld", h, m, s);
    else       std::snprintf(buf, sizeof buf, "%ld:%02ld", m, s);
    return buf;
}

std::string lag_label(double seconds) {
    char buf[64];
    if (seconds < kSecondsPerMinute) {
        std::snprintf(buf, sizeof buf, "%.1f s", seconds);
        return buf;
    }
    const long total = static_cast<long>(seconds + 0.5);
    std::snprintf(buf, sizeof buf, "%.0f s  (%ld:%02ld)", seconds,
                  total / kSecondsPerMinute, total % kSecondsPerMinute);
    return buf;
}

} // namespace gui::fmt
