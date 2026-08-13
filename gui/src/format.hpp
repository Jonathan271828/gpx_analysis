#pragma once

/// @file format.hpp
/// @brief Rendering a duration as text, in the several shapes the charts need.
///
/// Three charts each want a different compromise between brevity and precision
/// for the same quantity, so the variants live together rather than as private
/// copies that drift.

#include "types.hpp"

#include <string>

namespace gui::fmt {

/// @brief The shortest useful form, for an axis tick: "5s", "20m", "1h".
///
/// Rounds down to a single unit, so it is unambiguous only where the tick
/// positions themselves carry the scale.
/// @param seconds Duration in seconds.
/// @return The label.
std::string compact_duration(Long seconds);

/// @brief Elapsed time as a clock reading: "4:30", "1:22:15".
///
/// Hours appear only once there are any, so a short ride is not padded with a
/// leading zero field.
/// @param seconds Elapsed seconds; negatives are clamped to zero.
/// @return The reading.
std::string elapsed_clock(double seconds);

/// @brief A correlation lag: seconds, with the minutes reading once past one.
///
/// Ride structure repeats on the scale of minutes, and "1830 s" is harder to
/// place than "30:30".
/// @param seconds Lag in seconds.
/// @return The label.
std::string lag_label(double seconds);

} // namespace gui::fmt
