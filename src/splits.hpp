#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

#include <vector>

// ---------------------------------------------------------------------------
// Distance splits
//
// Break the ride into fixed-distance chunks (e.g. per kilometre) and summarise
// each — the classic split table a rider scans to see pacing.
// ---------------------------------------------------------------------------

namespace splits {

struct Split {
    Real start_km      = 0.0;
    Real end_km        = 0.0;
    Real dist_m        = 0.0;   // actual distance covered in this split
    Long dur_s         = 0;
    Real avg_speed_kmh = 0.0;
    Bool has_power     = false;
    Real avg_power_w   = 0.0;
    Bool has_hr        = false;
    Real avg_hr        = 0.0;
    Real gain_m        = 0.0;
    Real loss_m        = 0.0;
};

/// Split the track every `split_km` kilometres. Returns empty when split_km<=0
/// or the track is too short.
std::vector<Split> by_distance(const Track& track, const PowerAnalysis& pa,
                               Real split_km);

} // namespace splits
