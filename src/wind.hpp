#pragma once

/**
 * @file wind.hpp
 * @brief Historical wind: Open-Meteo fetch, JSON cache I/O, and a high-level
 *        per-track acquisition helper.
 *
 * Wind is fetched from the Open-Meteo Historical Weather API (ERA5 reanalysis),
 * which is free and needs no API key. All functions return false and set `err`
 * on failure; they never throw or crash the caller.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // WindData, Track

#include <string>

namespace wind {

/** @brief Where wind data comes from (chosen by --wind / --wind-cache / --wind-file). */
enum class Mode {
    Off,    /**< No wind applied. */
    Fetch,  /**< Fetch from Open-Meteo each run. */
    Cache,  /**< Fetch once, cache to disk, reuse thereafter. */
    File    /**< Load from a local JSON file (offline). */
};

/**
 * @brief Fetch hourly wind for a point and UTC date range from Open-Meteo.
 * @param lat        Latitude (deg).
 * @param lon        Longitude (deg).
 * @param start_date First date, "YYYY-MM-DD".
 * @param end_date   Last date, "YYYY-MM-DD".
 * @param out        Out: the fetched wind data.
 * @param err        Out: error description on failure.
 * @return True on success. Speed is m/s; direction is the "blows from" bearing.
 */
Bool fetch_open_meteo(Real lat, Real lon,
                      const std::string& start_date,
                      const std::string& end_date,
                      WindData& out, std::string& err);

/**
 * @brief Load wind data from a JSON file in Open-Meteo shape (the cache format).
 * @param path Path to the JSON file.
 * @param out  Out: the loaded wind data.
 * @param err  Out: error description on failure.
 * @return True on success.
 */
Bool load_json(const std::string& path, WindData& out, std::string& err);

/**
 * @brief Save wind data to a JSON file in Open-Meteo shape (on-disk cache).
 * @param path Path to write.
 * @param in   The wind data to save.
 * @param err  Out: error description on failure.
 * @return True on success.
 */
Bool save_json(const std::string& path, const WindData& in, std::string& err);

/**
 * @brief High-level per-track wind acquisition.
 *
 * Honours @p mode, fetching / loading / caching as needed (logging progress),
 * and returns the data (invalid on failure so callers can fall back to no wind).
 * @param mode        Where to source the data.
 * @param path        Cache/file base path (per-track suffix added when needed).
 * @param track       The track (its centroid and dates drive the request).
 * @param track_index Index of the track within its file.
 * @param ntracks     Number of tracks in the file (for the path suffix).
 * @return The wind data; `.valid` is false if unavailable.
 */
WindData obtain(Mode mode, const std::string& path, const Track& track,
                Size track_index, Size ntracks);

} // namespace wind
