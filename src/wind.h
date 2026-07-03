#pragma once

#include "gpx_reader.h"   // WindData

#include <string>

// ---------------------------------------------------------------------------
// Wind data source (all network + JSON I/O lives here, not in gpx_reader.cpp).
//
// Wind is fetched from the Open-Meteo Historical Weather API (ERA5 reanalysis),
// which is free and needs no API key. All functions return false and set `err`
// on failure; they never throw or crash the caller.
// ---------------------------------------------------------------------------

/// Fetch hourly wind for a single point and UTC date range from Open-Meteo.
/// Dates are "YYYY-MM-DD". Wind speed is returned in m/s and direction is the
/// meteorological "blows from" bearing in degrees.
bool fetch_open_meteo_wind(double lat, double lon,
                           const std::string& start_date,
                           const std::string& end_date,
                           WindData& out, std::string& err);

/// Load wind data from a JSON file in Open-Meteo shape (also the cache format).
bool load_wind_json(const std::string& path, WindData& out, std::string& err);

/// Save wind data to a JSON file in Open-Meteo shape (used as an on-disk cache).
bool save_wind_json(const std::string& path, const WindData& in, std::string& err);
