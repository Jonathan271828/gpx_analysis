#pragma once

/**
 * @file quadrant.hpp
 * @brief Quadrant analysis: time split across pedal-force × pedal-velocity
 *        quadrants, revealing the muscular vs cardiovascular character of a ride.
 *
 * The crosshair sits at FTP and a reference cadence, using average effective
 * pedal force (AEPF = P / CPV) and circumferential pedal velocity
 * (CPV = cadence * 2*pi * crank / 60). Requires cadence data.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

namespace quadrant {

/**
 * @brief Time-in-quadrant result.
 *
 * @ref seconds is indexed Q1..Q4: Q1 high force / high cadence, Q2 high force /
 * low cadence, Q3 low force / low cadence, Q4 low force / high cadence.
 */
struct Quadrants {
    Bool valid            = false;        /**< True if the track had cadence. */
    Real cpv_threshold_ms = 0.0;          /**< Crosshair pedal velocity (m/s). */
    Real aepf_threshold_n = 0.0;          /**< Crosshair pedal force (N). */
    Real avg_cadence_rpm  = 0.0;          /**< Average cadence over pedalling time (rpm). */
    Real seconds[4]       = {0, 0, 0, 0}; /**< Time in Q1, Q2, Q3, Q4 (s). */
    Real total_s          = 0.0;          /**< Total pedalling time counted (s). */
};

/**
 * @brief Analyse the ride into force/cadence quadrants.
 * @param track          The track to analyse.
 * @param pa             Power analysis (supplies the per-step power series).
 * @param ftp_w          FTP setting the force crosshair (W).
 * @param crank_length_m Crank-arm length (m), e.g. 0.1725.
 * @return Filled Quadrants; valid == false when the track has no cadence.
 */
Quadrants analyse(const Track& track, const PowerAnalysis& pa,
                  Real ftp_w, Real crank_length_m);

} // namespace quadrant
