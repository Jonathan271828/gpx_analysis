#pragma once

/**
 * @file peaks.hpp
 * @brief Peak-power efforts: the best average power over each of a set of
 *        durations, together with where in the ride it occurred.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

#include <string>
#include <vector>

namespace peaks {

/** @brief The best effort over a single duration. */
struct PeakEffort {
    Long        duration_s     = 0;     /**< Window length (s). */
    Real        avg_power_w     = 0.0;  /**< Best average power over that window (W). */
    Long        start_offset_s  = 0;    /**< Seconds from the ride start to the window. */
    std::string start_time;             /**< ISO timestamp of the window start. */
    Bool        measured        = false;/**< True if computed from measured power. */
};

/**
 * @brief Best effort for each requested duration.
 * @param track       The track to analyse.
 * @param pa          Power analysis (per-step power + timestamps).
 * @param durations_s Window lengths to evaluate (s); longer-than-ride skipped.
 * @return One PeakEffort per emitted duration, ascending. Uses measured power
 *         when the track carries it, otherwise the estimate.
 */
std::vector<PeakEffort> best_efforts(const Track& track, const PowerAnalysis& pa,
                                     const std::vector<Long>& durations_s);

} // namespace peaks
