#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, TrackStats, PowerAnalysis, PowerCurve, PowerHistogram
#include "signal.hpp"       // SpectralResult

#include <string>

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
                         const std::string& unit, const SpectralResult& sr);

} // namespace io
