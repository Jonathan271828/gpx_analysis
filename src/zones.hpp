#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Training-zone distributions
//
// Time spent in each power zone (from FTP) or heart-rate zone (from LTHR or
// max HR). Answers "was this endurance, tempo or threshold work?".
// ---------------------------------------------------------------------------

namespace zones {

struct Zone {
    std::string label;
    Real        lo      = 0.0;   // lower bound (W or bpm)
    Real        hi      = 0.0;   // upper bound; < 0 means open-topped
    Real        seconds = 0.0;
};

struct ZoneTable {
    Bool              valid = false;
    std::string       kind;      // "power" or "heart rate"
    std::string       basis;     // e.g. "FTP 305 W" or "LTHR 165 bpm"
    Real              total_s = 0.0;
    std::vector<Zone> zones;
};

/// Coggan 7-zone power distribution from FTP (watts).
ZoneTable power_zones(const Track& track, const PowerAnalysis& pa, Real ftp_w);

/// 5-zone heart-rate distribution. Uses %LTHR when `lthr > 0`, otherwise
/// %max-HR when `max_hr > 0`; returns valid == false if neither is given or
/// the track carries no heart rate.
ZoneTable hr_zones(const Track& track, const PowerAnalysis& pa,
                   Real lthr, Real max_hr);

} // namespace zones
