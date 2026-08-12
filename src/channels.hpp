#pragma once

/**
 * @file channels.hpp
 * @brief Extraction of per-sample time series (velocity, power, HR, cadence)
 *        from a track, in the form the autocorrelation/spectrum code expects.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackStats, PowerAnalysis

#include <string>
#include <vector>

namespace channels {

/** @brief One named (time, value) series ready for spectral analysis. */
struct Channel {
    std::string       name;   /**< File-name stem, e.g. "velocity". */
    std::string       unit;   /**< Unit label, e.g. "km/h". */
    std::vector<Real> t_s;    /**< Sample times (s from the ride start). */
    std::vector<Real> value;  /**< Sample values in @ref unit. */
};

/**
 * @brief Extract every channel the track carries.
 * @param track The track to read.
 * @param stats Its statistics (tells which sensor channels are present).
 * @param pa    Power analysis (supplies velocity and estimated power series).
 * @return The available channels; ones the track lacks are omitted. Only
 *         samples with a valid timestamp and a present value are included.
 */
std::vector<Channel> extract(const Track& track, const TrackStats& stats,
                             const PowerAnalysis& pa);

} // namespace channels
