#pragma once

/// @file span.hpp
/// @brief A closed interval on a plot axis.

#include <cmath>

namespace gui {

/// @brief The stretch of an axis a set of plots is showing.
///
/// Deliberately carries no unit: the signal plots span seconds, the comparison
/// plots span either kilometres or seconds depending on what the rides are
/// being lined up by, and the linking machinery is the same for all of them.
///
/// Grown with include() rather than assigned, so "no values yet" is a state the
/// type knows about. Every chart needs the same two steps -- find the range of
/// what it is about to draw, then leave a margin so the extremes do not sit on
/// the frame -- and each had written them itself.
struct Span {
    double lo = 0.0;
    double hi = 0.0;
    bool   set = false;   ///< Whether any value has been included.

    /// @brief True before any value has been included, or if degenerate.
    bool empty() const { return !set || !(hi > lo); }

    /// @brief Grow to cover @p v. Non-finite values are ignored.
    /// @param v The value to cover.
    /// @return This span, for chaining.
    Span& include(double v) {
        if (!std::isfinite(v)) return *this;
        if (!set) { lo = hi = v; set = true; return *this; }
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        return *this;
    }

    /// @brief Grow to cover all of @p other.
    /// @param other The span to cover.
    /// @return This span, for chaining.
    Span& include(const Span& other) {
        if (!other.set) return *this;
        include(other.lo);
        include(other.hi);
        return *this;
    }

    /// @brief This span widened at both ends.
    ///
    /// A zero-width span is widened about its value instead, since a constant
    /// trace would otherwise collapse the axis to nothing.
    ///
    /// @param fraction Margin at each end, as a fraction of the width.
    /// @return The widened span; an unset span is returned unchanged.
    Span padded(double fraction) const {
        if (!set) return *this;
        const double width  = hi - lo;
        const double margin = (width > 0.0 ? width : std::max(1.0, std::abs(hi)))
                              * fraction;
        Span out = *this;
        out.lo -= margin;
        out.hi += margin;
        return out;
    }
};

} // namespace gui
