#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // PowerCurve, Track, PowerAnalysis

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Critical-power model
//
// Fits the two-parameter model  P(t) = W'/t + CP  to the mean-maximal power
// curve: CP (sustainable aerobic power) and W' (the finite anaerobic work
// capacity above CP). Also produces a W'-balance time series showing how deep
// into that reserve the ride went.
// ---------------------------------------------------------------------------

namespace cp {

struct CpFit {
    Bool              valid      = false;
    Bool              measured   = false;   // fitted to measured vs estimated curve
    Real              cp_w       = 0.0;
    Real              w_prime_j  = 0.0;
    Real              est_ftp_w  = 0.0;      // ~ CP
    Int               n_points   = 0;
    std::vector<Long> used_durations;        // efforts the fit used (s)
};

/// Fit CP and W' by linear regression of best power against 1/duration over
/// mid-range efforts (2–20 min). Returns valid == false with < 2 usable points.
CpFit fit(const PowerCurve& curve);

// One point of the W'-balance time series (Skiba–Clarke integral model).
struct WbalSample {
    Long t_s     = 0;   // elapsed seconds
    Real wbal_j  = 0.0; // remaining anaerobic reserve (J)
    Real power_w = 0.0;
};

/// Integrate the W'-balance over the ride: it depletes at (P - CP) above CP and
/// refills toward W' in proportion to the deficit below CP.
std::vector<WbalSample> wbal_series(const Track& track, const PowerAnalysis& pa,
                                    Real cp_w, Real w_prime_j);

// How hard the anaerobic reserve was used ("matches burned").
struct MatchStats {
    Bool valid   = false;
    Int  matches = 0;     // distinct deep expenditures of W'
    Real min_j   = 0.0;   // lowest W'-balance reached
    Real min_pct = 0.0;   // ... as a % of W'
    Real end_j   = 0.0;   // W'-balance at the finish
    Real end_pct = 0.0;
};

/// Count "matches" from a W'-balance series: a match is a dip below 50 % of W'
/// after having recovered above 75 % (hysteresis avoids double-counting).
MatchStats count_matches(const std::vector<WbalSample>& series, Real w_prime_j);

} // namespace cp
