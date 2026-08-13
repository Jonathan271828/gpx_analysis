#pragma once

/// @file compare_view.hpp
/// @brief Several rides' channels on shared axes, one line per ride.

#include "analysis.hpp"   // DistanceAxis
#include "channels.hpp"   // channels::Channel
#include "span.hpp"

#include <string>
#include <vector>

namespace gui {

/// @brief What the rides are lined up by.
enum class CompareAxis {
    Distance,  ///< Kilometres covered: the same climb falls at the same x.
    Elapsed    ///< Seconds since the start: the same point in the effort.
};

/// @brief One ride's contribution to a comparison.
///
/// Holds pointers, not copies: a comparison is redrawn every frame from files
/// that already own their analysis, and duplicating several rides' worth of
/// channels each frame would be pure waste. The referents must outlive the
/// draw call, which they do -- they are the open files.
struct CompareRide {
    std::string                           label;       ///< Tab title of the file.
    const std::vector<channels::Channel>* channels = nullptr;
    const DistanceAxis*                   distance = nullptr;
};

/// @brief The channel names to offer, being every name any ride carries.
///
/// A union rather than an intersection: two of four rides having no cadence is
/// a fact about those rides worth seeing, not a reason to hide the channel from
/// the two that recorded it.
///
/// @param rides The rides being compared.
/// @return The names, in the order they are first met.
std::vector<std::string> compare_channels(const std::vector<CompareRide>& rides);

/// @brief Draw one plot per selected channel, each holding every ride that has
///        it, on a shared x axis.
///
/// Stacked per channel rather than one plot per ride, because the question is
/// how the rides differ in one quantity; and one plot per channel is possible
/// here -- unlike the signals view -- precisely because the same channel across
/// rides shares a unit and so can share a y axis.
///
/// Colour identifies the *ride* and is held across every plot, so one line can
/// be followed down the stack. That is the opposite of the signals view, where
/// colour identifies the channel, which is why the legend names the files.
///
/// @param rides    The rides; those lacking a channel are named under its plot.
/// @param names    Channel names, from compare_channels().
/// @param selected One flag per name; entries past its end count as off.
/// @param axis     What to line the rides up by.
/// @param range    In/out: the shared x span, in kilometres or seconds to match
///                 @p axis. Pass an empty Span to fit the data. Reset it when
///                 @p axis changes -- the numbers mean something else after.
/// @param width    Plot width in pixels.
/// @param height   Height of each individual plot in pixels.
void draw_compare_plots(const std::vector<CompareRide>& rides,
                        const std::vector<std::string>& names,
                        const std::vector<char>& selected,
                        CompareAxis axis, Span& range,
                        float width, float height);

} // namespace gui
