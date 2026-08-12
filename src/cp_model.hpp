#pragma once

/**
 * @file cp_model.hpp
 * @brief Critical-power model: fit CP and W' from the mean-maximal power curve,
 *        integrate the W'-balance over the ride, and count anaerobic "matches".
 *
 * The two-parameter model is P(t) = W'/t + CP, where CP is the sustainable
 * aerobic power and W' the finite anaerobic work capacity above CP.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // PowerCurve, Track, PowerAnalysis

#include <string>
#include <vector>

namespace cp {

/** @brief Result of fitting the critical-power model. */
struct CpFit {
    Bool              valid      = false; /**< True if the fit succeeded. */
    Bool              measured   = false; /**< Fitted to measured vs estimated curve. */
    Real              cp_w       = 0.0;   /**< Critical power (W). */
    Real              w_prime_j  = 0.0;   /**< Anaerobic work capacity W' (J). */
    Real              est_ftp_w  = 0.0;   /**< Estimated FTP (~ CP) (W). */
    Int               n_points   = 0;     /**< Number of efforts used in the fit. */
    std::vector<Long> used_durations;     /**< Effort durations used (s). */
};

/**
 * @brief Fit CP and W' by linear regression of best power against 1/duration
 *        over mid-range efforts (2-20 min).
 * @param curve The mean-maximal power curve to fit.
 * @return Filled CpFit; valid == false with fewer than 2 usable points.
 */
CpFit fit(const PowerCurve& curve);

/** @brief One point of the W'-balance time series (Skiba–Clarke integral model). */
struct WbalSample {
    Long t_s     = 0;   /**< Elapsed seconds. */
    Real wbal_j  = 0.0; /**< Remaining anaerobic reserve (J). */
    Real power_w = 0.0; /**< Power on the step (W). */
};

/**
 * @brief Integrate the W'-balance over the ride.
 *
 * The reserve depletes at (P - CP) above CP and refills toward W' in proportion
 * to the deficit below CP.
 * @param track     The track.
 * @param pa        Power analysis (per-step power + timestamps).
 * @param cp_w      Critical power (W).
 * @param w_prime_j Anaerobic capacity W' (J).
 * @return The W'-balance series; empty for non-positive CP/W'.
 */
std::vector<WbalSample> wbal_series(const Track& track, const PowerAnalysis& pa,
                                    Real cp_w, Real w_prime_j);

/** @brief How hard the anaerobic reserve was used across the ride. */
struct MatchStats {
    Bool valid   = false; /**< True if the series was non-empty. */
    Int  matches = 0;     /**< Distinct deep expenditures of W' ("matches burned"). */
    Real min_j   = 0.0;   /**< Lowest W'-balance reached (J). */
    Real min_pct = 0.0;   /**< Lowest balance as a % of W'. */
    Real end_j   = 0.0;   /**< W'-balance at the finish (J). */
    Real end_pct = 0.0;   /**< Ending balance as a % of W'. */
};

/**
 * @brief Count "matches" from a W'-balance series.
 *
 * A match is a dip below 50 % of W' after having recovered above 75 %
 * (hysteresis avoids double-counting a single deep effort).
 * @param series    A W'-balance series from wbal_series().
 * @param w_prime_j The W' the series was computed with (J).
 * @return Filled MatchStats; valid == false for an empty series.
 */
MatchStats count_matches(const std::vector<WbalSample>& series, Real w_prime_j);

} // namespace cp
