#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

// ---------------------------------------------------------------------------
// Quadrant analysis (pedal force vs pedal velocity)
//
// Splits riding time into four quadrants around a crosshair set at FTP and a
// reference cadence, using the average effective pedal force (AEPF = P / CPV)
// and circumferential pedal velocity (CPV = cadence * 2*pi * crank / 60):
//   Q1 high force / high cadence   Q2 high force / low cadence (grinding)
//   Q4 low force  / high cadence   Q3 low force  / low cadence (easy)
// Reveals the muscular vs cardiovascular character of the ride. Needs cadence.
// ---------------------------------------------------------------------------

namespace quadrant {

struct Quadrants {
    Bool valid            = false;
    Real cpv_threshold_ms = 0.0;   // crosshair pedal velocity (m/s)
    Real aepf_threshold_n = 0.0;   // crosshair pedal force (N)
    Real avg_cadence_rpm  = 0.0;
    Real seconds[4]       = {0, 0, 0, 0};  // Q1, Q2, Q3, Q4
    Real total_s          = 0.0;
};

/// Analyse the ride into quadrants. `crank_length_m` is the crank arm length
/// (e.g. 0.1725). Returns valid == false when the track has no cadence.
Quadrants analyse(const Track& track, const PowerAnalysis& pa,
                  Real ftp_w, Real crank_length_m);

} // namespace quadrant
