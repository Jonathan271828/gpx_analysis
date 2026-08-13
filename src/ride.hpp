#pragma once

/**
 * @file ride.hpp
 * @brief Walking a ride step by step: the vocabulary every metric shares.
 *
 * Almost every metric here is a loop over consecutive track points asking the
 * same three questions -- how long was this step, was the rider actually riding
 * during it, and how much power did they produce. Each module had answered them
 * itself, which is how five byte-identical copies of the same helper came to
 * exist and how one of them came to use a bare `20` where the others used a
 * named constant.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackPoint, PowerAnalysis

#include <vector>

namespace ride {

/**
 * @brief Longest step still counted as riding, in seconds.
 *
 * Beyond this the rider was stopped -- at a light, in a cafe, or the recording
 * dropped out -- and the interval belongs to no zone, no average and no
 * distribution. Sliding-window searches for a best effort deliberately do *not*
 * apply this: a window straddling a stop should be diluted by the stopped time,
 * not silently closed up as though the rider had teleported.
 */
constexpr Long kStopSeconds = 20;

/**
 * @brief Duration of the step arriving at point @p i.
 * @param pa Power analysis, for the per-point elapsed times.
 * @param i  Index of the step's later point; must be >= 1.
 * @return The step's length in seconds, or 0 when either timestamp is unusable
 *         or time did not advance.
 */
Real step_seconds(const PowerAnalysis& pa, Size i);

/**
 * @brief Whether the step arriving at @p i counts as riding.
 * @param pa Power analysis, for the per-point elapsed times.
 * @param i  Index of the step's later point; must be >= 1.
 * @return True when the step has a usable, positive duration no longer than
 *         @ref kStopSeconds.
 */
Bool is_riding(const PowerAnalysis& pa, Size i);

/**
 * @brief Power on the step arriving at point @p i.
 *
 * With @p prefer_measured, the mean of whichever endpoints carry a recorded
 * `<power>` value, which is how a power meter's samples are read onto steps;
 * a step with neither endpoint recorded contributes nothing. Otherwise the
 * modelled estimate for that step.
 *
 * @param pts             The track's points.
 * @param pa              Power analysis, for the estimated series.
 * @param i               Index of the step's later point; must be >= 1.
 * @param prefer_measured Whether to read the recorded channel.
 * @return Power in watts.
 */
Real step_power(const std::vector<TrackPoint>& pts, const PowerAnalysis& pa,
                Size i, Bool prefer_measured);

/**
 * @brief A ride reduced to what a best-effort search needs.
 *
 * Cumulative riding seconds and cumulative work, indexed by point and both zero
 * at the start, so the mean power over any window is two subtractions. Stopped
 * time is included: a window straddling a cafe stop should be diluted by it,
 * not closed up as though the rider had teleported across it.
 */
struct Cumulative {
    std::vector<Real> seconds;  /**< Elapsed seconds up to each point. */
    std::vector<Real> joules;   /**< Work done up to each point (J). */

    /** @brief Total elapsed time; 0 for an empty ride. */
    Real total_seconds() const { return seconds.empty() ? 0.0 : seconds.back(); }
    /** @brief Total work; 0 for an empty ride. */
    Real total_joules()  const { return joules.empty()  ? 0.0 : joules.back(); }
};

/**
 * @brief Accumulate time and work across a track.
 * @param track           The track.
 * @param pa              Power analysis, for times and the estimated series.
 * @param prefer_measured Whether to read the recorded power channel.
 * @return The cumulative series, both the length of the point list.
 */
Cumulative accumulate(const Track& track, const PowerAnalysis& pa,
                      Bool prefer_measured);

/** @brief The best window a search found. */
struct Window {
    Real mean_power_w = 0.0;    /**< Mean power over it (W). */
    Size begin        = 0;      /**< Index of its first point. */
    Size end          = 0;      /**< Index of its last point. */
    Bool found        = false;  /**< False when no window was long enough. */
};

/**
 * @brief Best mean power over a window of at least @p duration_s, starting at
 *        or after @p from.
 *
 * A two-pointer sweep. The windows are "at least" the duration rather than
 * exactly it because the samples are irregular; the upper pointer never
 * rewinds, so each start gets the shortest window that qualifies, which is the
 * closest available approximation to an exact one.
 *
 * @param c          Cumulative series from accumulate().
 * @param from       Earliest index the window may start at.
 * @param duration_s Minimum window length in seconds.
 * @return The best window; found == false when the ride affords none.
 */
Window best_window(const Cumulative& c, Size from, Long duration_s);

/**
 * @brief Mean power over the window @p w, read from a different accumulation.
 *
 * Lets a search driven by one power series report the other over the same
 * window -- which is how the power curve reports measured power for the window
 * its estimated series picked.
 *
 * @param c Cumulative series to read.
 * @param w Window to read over.
 * @return Mean power in watts; 0 when the window has no duration.
 */
Real mean_power_over(const Cumulative& c, const Window& w);

} // namespace ride
