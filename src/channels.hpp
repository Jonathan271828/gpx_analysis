#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackStats, PowerAnalysis

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Time-dependent channel extraction
//
// Pulls the per-sample series (velocity, power, heart rate, cadence) out of a
// track in the (time, value) form the autocorrelation/spectrum code expects.
// ---------------------------------------------------------------------------

namespace channels {

struct Channel {
    std::string       name;   // file-name stem, e.g. "velocity"
    std::string       unit;   // e.g. "km/h"
    std::vector<Real> t_s;    // sample times (s from start)
    std::vector<Real> value;  // sample values
};

/// Extract every channel the track carries. Only samples with a valid
/// timestamp and a present value are included; absent channels are omitted.
std::vector<Channel> extract(const Track& track, const TrackStats& stats,
                             const PowerAnalysis& pa);

} // namespace channels
