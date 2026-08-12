#pragma once

/**
 * @file screen_output.hpp
 * @brief Console report: every function that prints a human-readable section to
 *        stdout. These functions only format and print — they never compute or
 *        write files.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackStats, Hill, BestSegment, PowerStats, PowerParams
#include "metrics.hpp"      // TrainingLoad, Decoupling
#include "zones.hpp"        // ZoneTable
#include "splits.hpp"       // Split
#include "cp_model.hpp"     // CpFit, MatchStats
#include "trends.hpp"       // TrendPoint
#include "peaks.hpp"        // PeakEffort
#include "quadrant.hpp"     // Quadrants

#include <string>
#include <vector>

namespace io {

/** @brief Print the first @p max_print track points. */
void print_track_points(const Track& track, Size max_print);
/** @brief Print the track statistics block. */
void print_stats(const Track& track, const TrackStats& s);
/** @brief Print the hills table (distance, grade, power, VAM, category). */
void print_hills(const std::vector<Hill>& hills);
/** @brief Print one fastest-segment block, titled with @p label. */
void print_best_segment(const BestSegment& seg, const std::string& label);
/** @brief Print the estimated-power block. @p body_mass_kg adds W/kg (0 = skip). */
void print_power_stats(const PowerStats& ps, const PowerParams& pp, Real body_mass_kg);

// Training analysis
/** @brief Print the training-load block (NP/IF/TSS/VI, energy, W/kg, EF). */
void print_training_load(const metrics::TrainingLoad& t);
/** @brief Print aerobic decoupling (skipped silently without heart rate). */
void print_decoupling(const metrics::Decoupling& d);
/** @brief Print a zone-distribution table (power or heart rate). */
void print_zone_table(const zones::ZoneTable& z);
/** @brief Print the distance-split table. */
void print_splits(const std::vector<splits::Split>& sp);
/** @brief Print the critical-power fit (CP / W'). */
void print_cp(const cp::CpFit& f);
/** @brief Print the anaerobic-reserve "matches" summary. */
void print_matches(const cp::MatchStats& m);
/** @brief Print the peak-power efforts table. */
void print_peaks(const std::vector<peaks::PeakEffort>& pk);
/** @brief Print the quadrant analysis (force × cadence). */
void print_quadrant(const quadrant::Quadrants& q);
/** @brief Print the multi-ride training trend (CTL / ATL / TSB). */
void print_trends(const std::vector<trends::TrendPoint>& tp);

} // namespace io
