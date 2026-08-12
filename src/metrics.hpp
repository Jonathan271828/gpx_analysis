#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

// ---------------------------------------------------------------------------
// Training-analysis metrics
//
// Load-oriented numbers a rider uses to judge how hard a ride was and how the
// body responded: Normalized Power, Intensity Factor, Training Stress Score,
// Variability Index, energy, watts-per-kilo, and aerobic decoupling.
// Everything is derived from the per-step power series in PowerAnalysis (the
// measured <power> when the track carries it, otherwise the estimate).
// ---------------------------------------------------------------------------

namespace metrics {

struct TrainingLoad {
    Bool valid         = false;
    Bool from_measured = false;  // true when computed from a real power meter
    Real avg_power_w   = 0.0;
    Real np_w          = 0.0;    // Normalized Power
    Real if_           = 0.0;    // Intensity Factor = NP / FTP
    Real tss           = 0.0;    // Training Stress Score
    Real vi            = 0.0;    // Variability Index = NP / avg power
    Real ftp_w         = 0.0;
    Real kj            = 0.0;    // mechanical work
    Real kcal          = 0.0;    // ~ kj for cycling (see note in .cpp)
    Real body_mass_kg  = 0.0;
    Real avg_wkg       = 0.0;
    Real np_wkg        = 0.0;
    Long moving_s      = 0;      // moving time used (stops excluded)
};

/// Compute the training-load summary for a track. `ftp_w` and `body_mass_kg`
/// come from the CLI. Returns valid == false when there is no usable power.
TrainingLoad training_load(const Track& track, const PowerAnalysis& pa,
                           Real ftp_w, Real body_mass_kg);

struct Decoupling {
    Bool valid        = false;
    Real first_ratio  = 0.0;   // (avg power / avg HR) over the first moving half
    Real second_ratio = 0.0;   // ... and the second half
    Real pct          = 0.0;   // (first - second)/first * 100; + = HR drifted up
};

/// Aerobic decoupling (Pw:Hr): how much the power-to-heart-rate ratio drifted
/// from the first half of the ride to the second. Needs heart-rate data.
Decoupling decoupling(const Track& track, const PowerAnalysis& pa);

} // namespace metrics
