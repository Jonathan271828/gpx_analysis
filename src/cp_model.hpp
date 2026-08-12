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

} // namespace cp
