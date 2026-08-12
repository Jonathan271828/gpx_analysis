#include "wind.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Parse an Open-Meteo hourly timestamp ("2026-03-31T09:00") as UTC.
/// Returns -1 on failure.
Time parse_hourly_time(const std::string& s) {
    std::tm tm{};
    if (!strptime(s.c_str(), "%Y-%m-%dT%H:%M", &tm)) return -1;
    tm.tm_isdst = 0;
    return timegm(&tm);   // interpret as UTC (Linux/glibc)
}

/// HTTP GET by invoking the `curl` command-line tool (no libcurl dependency).
/// Collects the response body; returns false with `err` set on any failure.
Bool http_get(const std::string& url, std::string& body, std::string& err) {
    // URLs we build never contain single quotes; refuse any that do so the
    // single-quoted shell argument below cannot be broken out of.
    if (url.find('\'') != std::string::npos) { err = "unsafe URL"; return false; }

    // -sS: quiet but show errors; -f: non-zero exit on HTTP >= 400
    const std::string cmd = "curl -sS -f --max-time 30 -A 'gpx_reader/1.0' '"
                          + url + "'";
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) { err = "failed to launch curl"; return false; }

    Char buf[4096];
    Size nread;
    while ((nread = std::fread(buf, 1, sizeof(buf), pipe)) > 0)
        body.append(buf, nread);

    const Int rc = ::pclose(pipe);
    if (rc != 0) {
        err = "curl failed (exit " + std::to_string(rc)
            + "); check network / that the date is in the ERA5 archive";
        return false;
    }
    return true;
}

/// Fill WindData from a parsed Open-Meteo JSON object (also the cache format).
Bool parse_open_meteo_json(const nlohmann::json& j, WindData& out, std::string& err) {
    try {
        if (!j.contains("hourly")) { err = "response missing 'hourly'"; return false; }
        const auto& h  = j.at("hourly");
        const auto& t  = h.at("time");
        const auto& sp = h.at("wind_speed_10m");
        const auto& di = h.at("wind_direction_10m");

        const Size n = t.size();
        if (sp.size() != n || di.size() != n) { err = "hourly arrays size mismatch"; return false; }

        out.times.clear();
        out.speed_ms.clear();
        out.dir_deg.clear();
        out.times.reserve(n);
        out.speed_ms.reserve(n);
        out.dir_deg.reserve(n);

        for (Size i = 0; i < n; ++i) {
            if (t[i].is_null() || sp[i].is_null() || di[i].is_null()) continue;
            const Time tt = parse_hourly_time(t[i].get<std::string>());
            if (tt < 0) continue;
            out.times.push_back(tt);
            out.speed_ms.push_back(sp[i].get<Real>());
            out.dir_deg.push_back(di[i].get<Real>());
        }

        out.valid = !out.times.empty();
        if (!out.valid) { err = "no usable hourly wind samples"; return false; }
        return true;
    } catch (const std::exception& e) {
        err = std::string("JSON parse error: ") + e.what();
        return false;
    }
}

} // namespace

namespace wind {

// ---------------------------------------------------------------------------
// fetch_open_meteo
// ---------------------------------------------------------------------------

Bool fetch_open_meteo(Real lat, Real lon,
                      const std::string& start_date,
                      const std::string& end_date,
                      WindData& out, std::string& err)
{
    std::ostringstream url;
    url << "https://archive-api.open-meteo.com/v1/archive"
        << "?latitude="   << lat
        << "&longitude="  << lon
        << "&start_date=" << start_date
        << "&end_date="   << end_date
        << "&hourly=wind_speed_10m,wind_direction_10m,wind_gusts_10m"
        << "&wind_speed_unit=ms&timezone=GMT";

    std::string body;
    if (!http_get(url.str(), body, err)) return false;

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(body);
    } catch (const std::exception& e) {
        err = std::string("JSON decode error: ") + e.what();
        return false;
    }

    return parse_open_meteo_json(j, out, err);
}

// ---------------------------------------------------------------------------
// load_json / save_json  (JSON cache in Open-Meteo shape)
// ---------------------------------------------------------------------------

Bool load_json(const std::string& path, WindData& out, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "cannot open " + path; return false; }

    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        err = std::string("JSON parse error: ") + e.what();
        return false;
    }
    return parse_open_meteo_json(j, out, err);
}

Bool save_json(const std::string& path, const WindData& in, std::string& err) {
    nlohmann::json j;
    j["hourly"]["time"]               = nlohmann::json::array();
    j["hourly"]["wind_speed_10m"]     = nlohmann::json::array();
    j["hourly"]["wind_direction_10m"] = nlohmann::json::array();

    for (Size i = 0; i < in.times.size(); ++i) {
        std::tm tm{};
        Time tt = in.times[i];
        gmtime_r(&tt, &tm);
        Char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M", &tm);
        j["hourly"]["time"].push_back(std::string(buf));
        j["hourly"]["wind_speed_10m"].push_back(in.speed_ms[i]);
        j["hourly"]["wind_direction_10m"].push_back(in.dir_deg[i]);
    }

    std::ofstream out(path);
    if (!out) { err = "cannot open " + path + " for writing"; return false; }
    out << j.dump(2) << "\n";
    return true;
}

// ---------------------------------------------------------------------------
// obtain — per-track wind acquisition (fetch / cache / file), with logging
// ---------------------------------------------------------------------------

WindData obtain(Mode mode, const std::string& path, const Track& track,
                Size track_index, Size ntracks)
{
    WindData wind;
    if (mode == Mode::Off) return wind;

    const auto& pts = track.points;
    if (pts.size() < 2 || pts.front().time.size() < 10 || pts.back().time.size() < 10) {
        std::cerr << "Wind: track has no usable coordinates/timestamps — skipping.\n";
        return wind;
    }

    // Request location = track centroid (ERA5's ~25 km grid makes finer pointless)
    Real lat_sum = 0.0, lon_sum = 0.0;
    for (const auto& p : pts) { lat_sum += p.lat; lon_sum += p.lon; }
    const Real lat = lat_sum / static_cast<Real>(pts.size());
    const Real lon = lon_sum / static_cast<Real>(pts.size());
    const std::string start_date = pts.front().time.substr(0, 10);
    const std::string end_date   = pts.back().time.substr(0, 10);

    // Per-track cache/file path (suffix with index when more than one track)
    const std::string p = (!path.empty() && ntracks > 1)
                        ? path + "." + std::to_string(track_index)
                        : path;

    std::string err;

    if (mode == Mode::File) {
        if (!load_json(p, wind, err))
            std::cerr << "Wind: could not load " << p << " (" << err << ") — no wind applied.\n";
        return wind;
    }

    if (mode == Mode::Cache && load_json(p, wind, err)) {
        std::cout << "Wind: loaded from cache " << p << "\n";
        return wind;
    }

    // Fetch from Open-Meteo (Fetch mode, or Cache miss)
    std::cout << "Wind: fetching from Open-Meteo (" << start_date
              << " .. " << end_date << ")...\n";
    if (!fetch_open_meteo(lat, lon, start_date, end_date, wind, err)) {
        std::cerr << "Wind: fetch failed (" << err << ") — no wind applied.\n";
        wind.valid = false;
        return wind;
    }
    if (mode == Mode::Cache) {
        std::string save_err;
        if (save_json(p, wind, save_err))
            std::cout << "Wind: cached to " << p << "\n";
        else
            std::cerr << "Wind: could not write cache " << p << " (" << save_err << ")\n";
    }
    return wind;
}

} // namespace wind
