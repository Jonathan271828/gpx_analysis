#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // WindData, Track

#include <string>

// ---------------------------------------------------------------------------
// Wind data source (all network + JSON I/O lives here, not in gpx_reader.cpp).
//
// Wind is fetched from the Open-Meteo Historical Weather API (ERA5 reanalysis),
// which is free and needs no API key. All functions return false and set `err`
// on failure; they never throw or crash the caller.
// ---------------------------------------------------------------------------

namespace wind {

/// Where wind data comes from (chosen by --wind / --wind-cache / --wind-file).
enum class Mode { Off, Fetch, Cache, File };

/// Fetch hourly wind for a single point and UTC date range from Open-Meteo.
/// Dates are "YYYY-MM-DD". Wind speed is returned in m/s and direction is the
/// meteorological "blows from" bearing in degrees.
Bool fetch_open_meteo(Real lat, Real lon,
                      const std::string& start_date,
                      const std::string& end_date,
                      WindData& out, std::string& err);

/// Load wind data from a JSON file in Open-Meteo shape (also the cache format).
Bool load_json(const std::string& path, WindData& out, std::string& err);

/// Save wind data to a JSON file in Open-Meteo shape (used as an on-disk cache).
Bool save_json(const std::string& path, const WindData& in, std::string& err);

/// High-level per-track wind acquisition: honours `mode`, fetches / loads /
/// caches as needed (logging progress), and returns the data (invalid on
/// failure so callers can fall back to no wind). `path` is the cache/file base.
WindData obtain(Mode mode, const std::string& path, const Track& track,
                Size track_index, Size ntracks);

} // namespace wind
