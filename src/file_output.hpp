#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackStats, PowerAnalysis, PowerCurve, PowerHistogram
#include "signal.hpp"       // SpectralResult
#include "cp_model.hpp"     // cp::WbalSample

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// File output
//
// Writers that serialise analysis results to data files (CSV / whitespace
// tables). Each returns false if the file could not be opened or written.
// ---------------------------------------------------------------------------

namespace io {

Bool write_power_csv(const std::string& path, const Track& track,
                     const PowerAnalysis& pa);

Bool write_xy_file(const std::string& path, const Track& track,
                   const TrackStats& stats, const PowerAnalysis& pa);

Bool write_power_curve_file(const std::string& path, const PowerCurve& curve);

Bool write_power_hist_file(const std::string& path, const PowerHistogram& hist);

Bool write_spectral_file(const std::string& path, const std::string& channel,
                         const std::string& unit, const signal::SpectralResult& sr);

/// Write a precomputed W'-balance series (cp::wbal_series). `cp_w`/`w_prime_j`
/// are recorded in the header. Columns: elapsed_s, wbal_j, power_w.
Bool write_wbal_file(const std::string& path,
                     const std::vector<cp::WbalSample>& series,
                     Real cp_w, Real w_prime_j);

} // namespace io
