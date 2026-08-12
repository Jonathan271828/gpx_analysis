#pragma once

#include "types.hpp"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Multi-ride training trend
//
// Turns a set of rides (each a date + Training Stress Score) into the classic
// fitness / fatigue / form curves:
//   CTL (Chronic Training Load)  — 42-day exponential average of daily TSS
//   ATL (Acute Training Load)    — 7-day exponential average
//   TSB (Training Stress Balance)— CTL - ATL, i.e. "form"
// ---------------------------------------------------------------------------

namespace trends {

struct RideLoad {
    std::string date;   // "YYYY-MM-DD"
    std::string label;  // ride/file name (for display)
    Real        tss = 0.0;
};

struct TrendPoint {
    std::string date;
    std::string label;
    Real        tss = 0.0;
    Real        ctl = 0.0;
    Real        atl = 0.0;
    Real        tsb = 0.0;
};

/// Compute CTL/ATL/TSB day by day across the span of the rides, returning one
/// row per ride day (form is measured as of that day). Returns empty on bad
/// input (unparseable dates, etc.).
std::vector<TrendPoint> progression(std::vector<RideLoad> rides);

} // namespace trends
