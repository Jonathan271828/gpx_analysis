#pragma once

/**
 * @file zones.hpp
 * @brief Training-zone distributions: time spent in each power zone (from FTP)
 *        or heart-rate zone (from LTHR or max HR).
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

#include <string>
#include <vector>

namespace zones {

/** @brief One zone: its label, bounds and accumulated time. */
struct Zone {
    std::string label;         /**< Human-readable label, e.g. "Z2 Endurance". */
    Real        lo      = 0.0; /**< Lower bound (W or bpm). */
    Real        hi      = 0.0; /**< Upper bound; < 0 means open-topped. */
    Real        seconds = 0.0; /**< Time spent in this zone (s). */
};

/** @brief A full zone distribution (power or heart rate). */
struct ZoneTable {
    Bool              valid = false;  /**< True if any time was accumulated. */
    std::string       kind;           /**< "power" or "heart rate". */
    std::string       basis;          /**< e.g. "FTP 305 W" or "LTHR 165 bpm". */
    Real              total_s = 0.0;  /**< Total time across all zones (s). */
    std::vector<Zone> zones;          /**< The zones in ascending order. */
};

/**
 * @brief Coggan 7-zone power distribution.
 * @param track The track to analyse.
 * @param pa    Power analysis (supplies the per-step power series).
 * @param ftp_w Functional threshold power (W).
 * @return The zone table; valid == false if no power or FTP.
 */
ZoneTable power_zones(const Track& track, const PowerAnalysis& pa, Real ftp_w);

/**
 * @brief 5-zone heart-rate distribution.
 * @param track  The track to analyse.
 * @param pa     Power analysis (supplies per-step timestamps).
 * @param lthr   Lactate-threshold HR (bpm); used as the reference when > 0.
 * @param max_hr Maximum HR (bpm); fallback reference when @p lthr is 0.
 * @return The zone table; valid == false if neither reference is given or the
 *         track carries no heart rate.
 */
ZoneTable hr_zones(const Track& track, const PowerAnalysis& pa,
                   Real lthr, Real max_hr);

} // namespace zones
