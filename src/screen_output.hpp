#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackStats, Hill, BestSegment, PowerStats, PowerParams

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
void print_power_stats(const PowerStats& ps, const PowerParams& pp);

} // namespace io
