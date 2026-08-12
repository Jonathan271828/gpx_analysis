#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackStats, Hill, BestSegment, PowerStats, PowerParams
#include "metrics.hpp"      // TrainingLoad, Decoupling
#include "zones.hpp"        // ZoneTable
#include "splits.hpp"       // Split
#include "cp_model.hpp"     // CpFit
#include "trends.hpp"       // TrendPoint

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Screen output
//
// Everything that prints a human-readable report to stdout. These functions
// only format and print; they never compute or write files.
// ---------------------------------------------------------------------------

namespace io {

void print_track_points(const Track& track, Size max_print);
void print_stats(const Track& track, const TrackStats& s);
void print_hills(const std::vector<Hill>& hills);
void print_best_segment(const BestSegment& seg, const std::string& label);
void print_power_stats(const PowerStats& ps, const PowerParams& pp, Real body_mass_kg);

// Training analysis
void print_training_load(const metrics::TrainingLoad& t);
void print_decoupling(const metrics::Decoupling& d);
void print_zone_table(const zones::ZoneTable& z);
void print_splits(const std::vector<splits::Split>& sp);
void print_cp(const cp::CpFit& f);
void print_trends(const std::vector<trends::TrendPoint>& tp);

} // namespace io
