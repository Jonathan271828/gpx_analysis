#pragma once

/// @file span.hpp
/// @brief A closed interval on a plot axis.

namespace gui {

/// @brief The stretch of an axis a set of plots is showing.
///
/// Deliberately carries no unit: the signal plots span seconds, the comparison
/// plots span either kilometres or seconds depending on what the rides are
/// being lined up by, and the linking machinery is the same for all of them.
struct Span {
    double lo = 0.0;
    double hi = 0.0;

    /// @brief True before a span has been set, or if it is degenerate.
    bool empty() const { return !(hi > lo); }
};

} // namespace gui
