#include "wind.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Parse an Open-Meteo hourly timestamp ("2026-03-31T09:00") as UTC.
/// Returns -1 on failure.
std::time_t parse_hourly_time(const std::string& s) {
    std::tm tm{};
    if (!strptime(s.c_str(), "%Y-%m-%dT%H:%M", &tm)) return -1;
    tm.tm_isdst = 0;
    return timegm(&tm);   // interpret as UTC (Linux/glibc)
}

/// HTTP GET by invoking the `curl` command-line tool (no libcurl dependency).
/// Collects the response body; returns false with `err` set on any failure.
bool http_get(const std::string& url, std::string& body, std::string& err) {
    // URLs we build never contain single quotes; refuse any that do so the
    // single-quoted shell argument below cannot be broken out of.
    if (url.find('\'') != std::string::npos) { err = "unsafe URL"; return false; }

    // -sS: quiet but show errors; -f: non-zero exit on HTTP >= 400
    const std::string cmd = "curl -sS -f --max-time 30 -A 'gpx_reader/1.0' '"
                          + url + "'";
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) { err = "failed to launch curl"; return false; }

    char buf[4096];
    std::size_t nread;
    while ((nread = std::fread(buf, 1, sizeof(buf), pipe)) > 0)
        body.append(buf, nread);

    const int rc = ::pclose(pipe);
    if (rc != 0) {
        err = "curl failed (exit " + std::to_string(rc)
            + "); check network / that the date is in the ERA5 archive";
        return false;
    }
    return true;
}

/// Fill WindData from a parsed Open-Meteo JSON object (also the cache format).
bool parse_open_meteo_json(const nlohmann::json& j, WindData& out, std::string& err) {
    try {
        if (!j.contains("hourly")) { err = "response missing 'hourly'"; return false; }
        const auto& h  = j.at("hourly");
        const auto& t  = h.at("time");
        const auto& sp = h.at("wind_speed_10m");
        const auto& di = h.at("wind_direction_10m");

        const std::size_t n = t.size();
        if (sp.size() != n || di.size() != n) { err = "hourly arrays size mismatch"; return false; }

        out.times.clear();
        out.speed_ms.clear();
        out.dir_deg.clear();
        out.times.reserve(n);
        out.speed_ms.reserve(n);
        out.dir_deg.reserve(n);

        for (std::size_t i = 0; i < n; ++i) {
            if (t[i].is_null() || sp[i].is_null() || di[i].is_null()) continue;
            const std::time_t tt = parse_hourly_time(t[i].get<std::string>());
            if (tt < 0) continue;
            out.times.push_back(tt);
            out.speed_ms.push_back(sp[i].get<double>());
            out.dir_deg.push_back(di[i].get<double>());
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

// ---------------------------------------------------------------------------
// fetch_open_meteo_wind
// ---------------------------------------------------------------------------

bool fetch_open_meteo_wind(double lat, double lon,
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
// load_wind_json / save_wind_json  (JSON cache in Open-Meteo shape)
// ---------------------------------------------------------------------------

bool load_wind_json(const std::string& path, WindData& out, std::string& err) {
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

bool save_wind_json(const std::string& path, const WindData& in, std::string& err) {
    nlohmann::json j;
    j["hourly"]["time"]               = nlohmann::json::array();
    j["hourly"]["wind_speed_10m"]     = nlohmann::json::array();
    j["hourly"]["wind_direction_10m"] = nlohmann::json::array();

    for (std::size_t i = 0; i < in.times.size(); ++i) {
        std::tm tm{};
        std::time_t tt = in.times[i];
        gmtime_r(&tt, &tm);
        char buf[32];
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
