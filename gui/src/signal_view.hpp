#pragma once

/// @file signal_view.hpp
/// @brief The ride's channels plotted raw, against elapsed time.
///
/// The spectral tabs answer "what repeats"; this one answers "what happened,
/// and when". Same channels, no transform.

#include "channels.hpp"   // channels::Channel

#include <vector>

namespace gui {

/// @brief The time span every signal plot is showing, shared between them.
///
/// The plots are stacked rather than overlaid, because a ride's channels span
/// wildly different ranges -- watts in the hundreds, gradient in single digits
/// -- and one y axis cannot serve them. Stacking costs the ability to read one
/// signal against another at a glance, which linking the x axes buys back:
/// zoom or pan any plot and the rest follow, so a vertical line through the
/// stack is one moment in the ride.
struct TimeRange {
    double begin_s = 0.0;
    double end_s   = 0.0;

    /// @brief True before a span has been set.
    bool empty() const { return !(end_s > begin_s); }
};

/// @brief Draw one plot per selected channel, stacked on a shared time axis.
///
/// @param channels All the track's channels.
/// @param selected One flag per channel; entries past its end count as off.
/// @param range    In/out: the shared x span. Pass a default-constructed
///                 TimeRange to fit the data; it is then updated by whatever
///                 the user zooms or pans to, and carried between frames.
/// @param width    Plot width in pixels.
/// @param height   Height of each individual plot in pixels.
void draw_signal_plots(const std::vector<channels::Channel>& channels,
                       const std::vector<char>& selected,
                       TimeRange& range, float width, float height);

} // namespace gui
