#pragma once

/**
 * @file splits.hpp
 * @brief Fixed-distance split table (per kilometre, per mile, ...): the classic
 *        pacing summary of a ride, one row per chunk.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

#include <vector>

namespace splits {

/** @brief Summary of one distance split. */
struct Split {
    Real start_km      = 0.0;   /**< Split start distance (km). */
    Real end_km        = 0.0;   /**< Split end distance (km). */
    Real dist_m        = 0.0;   /**< Actual distance covered in this split (m). */
    Long dur_s         = 0;     /**< Time spent in this split (s). */
    Real avg_speed_kmh = 0.0;   /**< Average speed (km/h). */
    Bool has_power     = false; /**< True if power was available. */
    Real avg_power_w   = 0.0;   /**< Average power (W). */
    Bool has_hr        = false; /**< True if heart rate was available. */
    Real avg_hr        = 0.0;   /**< Average heart rate (bpm). */
    Real gain_m        = 0.0;   /**< Elevation gained in the split (m). */
    Real loss_m        = 0.0;   /**< Elevation lost in the split (m). */
};

/**
 * @brief Split the track into `split_km`-kilometre chunks.
 * @param track    The track to split.
 * @param pa       Power analysis (supplies cumulative distance, time, power).
 * @param split_km Split length (km).
 * @return One Split per chunk; empty when split_km <= 0 or the track is too short.
 */
std::vector<Split> by_distance(const Track& track, const PowerAnalysis& pa,
                               Real split_km);

} // namespace splits
