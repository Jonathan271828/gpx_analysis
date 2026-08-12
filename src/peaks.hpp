#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Peak power efforts
//
// The best average power sustained over each requested duration, together with
// *where* in the ride it happened — e.g. "best 5-min: 312 W at 1h02m".
// ---------------------------------------------------------------------------

namespace peaks {

struct PeakEffort {
    Long        duration_s    = 0;
    Real        avg_power_w    = 0.0;
    Long        start_offset_s = 0;   // seconds from the ride start
    std::string start_time;           // ISO timestamp of the window start
    Bool        measured       = false;
};

/// Best effort for each duration in `durations_s` (durations longer than the
/// ride are skipped). Uses measured power when the track carries it, else the
/// estimate.
std::vector<PeakEffort> best_efforts(const Track& track, const PowerAnalysis& pa,
                                     const std::vector<Long>& durations_s);

} // namespace peaks
