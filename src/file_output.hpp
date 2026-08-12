#pragma once

/**
 * @file file_output.hpp
 * @brief Data-file writers: serialise analysis results to CSV / whitespace
 *        tables. Each returns false if the file could not be opened or written.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackStats, PowerAnalysis, PowerCurve, PowerHistogram
#include "signal.hpp"       // signal::SpectralResult
#include "cp_model.hpp"     // cp::WbalSample

#include <string>
#include <vector>

namespace io {

/**
 * @brief Write a per-point time-vs-power CSV.
 * @param path  Output path.
 * @param track The track (for timestamps and measured power).
 * @param pa    Power analysis (estimated power, elapsed time, headwind).
 * @return True on success.
 */
Bool write_power_csv(const std::string& path, const Track& track,
                     const PowerAnalysis& pa);

/**
 * @brief Write the full per-point XY table (`#`-commented header, NaN-padded).
 * @param path  Output path.
 * @param track The track.
 * @param stats Its statistics (tells which columns to emit).
 * @param pa    Power analysis (distance, speed, estimated power, headwind).
 * @return True on success.
 */
Bool write_xy_file(const std::string& path, const Track& track,
                   const TrackStats& stats, const PowerAnalysis& pa);

/**
 * @brief Write the mean-maximal power curve (duration vs watts).
 * @param path  Output path.
 * @param curve The power curve.
 * @return True on success.
 */
Bool write_power_curve_file(const std::string& path, const PowerCurve& curve);

/**
 * @brief Write the power histogram (band vs seconds in band).
 * @param path Output path.
 * @param hist The histogram.
 * @return True on success.
 */
Bool write_power_hist_file(const std::string& path, const PowerHistogram& hist);

/**
 * @brief Write a channel's autocorrelation + power spectrum (4-column table).
 * @param path    Output path.
 * @param channel Channel name (for the header).
 * @param unit    Channel unit (for the header).
 * @param sr      The spectral result.
 * @return True on success.
 */
Bool write_spectral_file(const std::string& path, const std::string& channel,
                         const std::string& unit, const signal::SpectralResult& sr);

/**
 * @brief Write a precomputed W'-balance series (from cp::wbal_series).
 * @param path      Output path.
 * @param series    The W'-balance samples.
 * @param cp_w      CP recorded in the header (W).
 * @param w_prime_j W' recorded in the header (J).
 * @return True on success.
 */
Bool write_wbal_file(const std::string& path,
                     const std::vector<cp::WbalSample>& series,
                     Real cp_w, Real w_prime_j);

} // namespace io
