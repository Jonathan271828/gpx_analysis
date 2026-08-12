#include "arg_parser.hpp"
#include "file_output.hpp"
#include "gpx_reader.hpp"
#include "io_base.hpp"
#include "screen_output.hpp"
#include "signal.hpp"
#include "wind.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Bring the wind-source mode enum (defined by the argument parser) into scope
// so obtain_wind() and main() can name it unqualified.
using arg_parser::WindMode;

// ---------------------------------------------------------------------------
// Extract the time-dependent channels (time in seconds + value) from a track.
// Only samples with a valid timestamp and a present value are included, so
// each channel is ready to hand to compute_acf_psd(). Channels the track does
// not carry come back empty and are skipped by the caller.
// ---------------------------------------------------------------------------

struct Channel {
    std::string         name;   // file-name stem, e.g. "velocity"
    std::string         unit;   // e.g. "km/h"
    std::vector<Real> t_s;    // sample times (s from start)
    std::vector<Real> value;  // sample values
};

static std::vector<Channel> extract_channels(const Track&         track,
                                             const TrackStats&    stats,
                                             const PowerAnalysis& pa)
{
    const auto&       pts = track.points;
    const Size n   = pts.size();

    Channel velocity{"velocity", "km/h", {}, {}};
    Channel est_power{"power", "W", {}, {}};
    Channel meas_power{"power_measured", "W", {}, {}};
    Channel hr{"hr", "bpm", {}, {}};
    Channel cadence{"cadence", "rpm", {}, {}};

    for (Size i = 0; i < n; ++i) {
        const Long t = (i < pa.t_offset_s.size()) ? pa.t_offset_s[i] : -1;
        if (t < 0) continue;
        const Real ts = static_cast<Real>(t);

        // Velocity and estimated power are step quantities (defined for i >= 1).
        if (i > 0 && pa.stats.valid) {
            velocity.t_s.push_back(ts);
            velocity.value.push_back(pa.speed_ms[i] * 3.6);   // m/s -> km/h
            est_power.t_s.push_back(ts);
            est_power.value.push_back(pa.point_power_w[i]);
        }
        // Sensor channels: only where the point actually carries the field.
        if (pts[i].has_power) {
            meas_power.t_s.push_back(ts);
            meas_power.value.push_back(static_cast<Real>(pts[i].power));
        }
        if (pts[i].has_hr) {
            hr.t_s.push_back(ts);
            hr.value.push_back(static_cast<Real>(pts[i].hr));
        }
        if (pts[i].has_cad) {
            cadence.t_s.push_back(ts);
            cadence.value.push_back(static_cast<Real>(pts[i].cad));
        }
    }

    std::vector<Channel> out;
    if (pa.stats.valid)  { out.push_back(std::move(velocity));
                           out.push_back(std::move(est_power)); }
    if (stats.has_power)   out.push_back(std::move(meas_power));
    if (stats.has_hr)      out.push_back(std::move(hr));
    if (stats.has_cad)     out.push_back(std::move(cadence));
    return out;
}

// ---------------------------------------------------------------------------
// Obtain wind data for a track (network fetch and/or JSON cache/file)
// ---------------------------------------------------------------------------

static WindData obtain_wind(WindMode mode, const std::string& path,
                            const Track& track, Size track_index,
                            Size ntracks)
{
    WindData wind;
    if (mode == WindMode::Off) return wind;

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

    if (mode == WindMode::File) {
        if (!load_wind_json(p, wind, err))
            std::cerr << "Wind: could not load " << p << " (" << err << ") — no wind applied.\n";
        return wind;
    }

    if (mode == WindMode::Cache && load_wind_json(p, wind, err)) {
        std::cout << "Wind: loaded from cache " << p << "\n";
        return wind;
    }

    // Fetch from Open-Meteo (Fetch mode, or Cache miss)
    std::cout << "Wind: fetching from Open-Meteo (" << start_date
              << " .. " << end_date << ")...\n";
    if (!fetch_open_meteo_wind(lat, lon, start_date, end_date, wind, err)) {
        std::cerr << "Wind: fetch failed (" << err << ") — no wind applied.\n";
        wind.valid = false;
        return wind;
    }
    if (mode == WindMode::Cache) {
        std::string save_err;
        if (save_wind_json(p, wind, save_err))
            std::cout << "Wind: cached to " << p << "\n";
        else
            std::cerr << "Wind: could not write cache " << p << " (" << save_err << ")\n";
    }
    return wind;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

Int main(Int argc, Char* argv[]) {
    // All command-line handling lives in the arg_parser module.
    arg_parser::Options opts;
    std::string parse_error;
    if (!arg_parser::parse(argc, argv, opts, parse_error)) {
        if (!parse_error.empty()) std::cerr << parse_error << "\n";
        arg_parser::print_usage(argv[0], std::cerr);
        return EXIT_FAILURE;
    }

    // Unpack the parsed options into the local names used throughout the run
    // (all read-only below).
    const std::string&       filepath           = opts.filepath;
    const Size               max_print          = opts.max_print;
    const std::vector<Real>& dist_windows       = opts.dist_windows;
    const std::vector<Long>& time_windows       = opts.time_windows;
    const PowerParams&       power              = opts.power;
    const std::string&       power_csv          = opts.power_csv;
    const std::string&       xy_path            = opts.xy_path;
    const std::string&       power_curve_path   = opts.power_curve_path;
    const std::string&       power_hist_path    = opts.power_hist_path;
    const Real               hist_bin_w         = opts.hist_bin_w;
    const std::string&       acf_velocity       = opts.acf_velocity;
    const std::string&       acf_power          = opts.acf_power;
    const std::string&       acf_power_measured = opts.acf_power_measured;
    const std::string&       acf_hr             = opts.acf_hr;
    const std::string&       acf_cadence        = opts.acf_cadence;
    const Real               acf_dt             = opts.acf_dt;
    const WindMode           wind_mode          = opts.wind_mode;
    const std::string&       wind_path          = opts.wind_path;

    // Parse GPX
    GpxReader reader;
    if (!reader.parse(filepath)) {
        std::cerr << "Error: " << reader.error_message() << "\n";
        return EXIT_FAILURE;
    }

    const GpxData& data = reader.data();

    std::cout << "GPX file   : " << filepath << "\n";
    if (!data.metadata_time.empty())
        std::cout << "Recorded   : " << data.metadata_time << "\n";
    std::cout << "Tracks     : " << data.tracks.size() << "\n";

    for (Size i = 0; i < data.tracks.size(); ++i) {
        const Track& track = data.tracks[i];

        // Track points
        io::print_track_points(track, max_print);

        // Overall statistics
        TrackStats stats = reader.compute_stats(i);
        io::print_stats(track, stats);

        // Wind data (fetched/loaded per track), then estimated power
        WindData wind = obtain_wind(wind_mode, wind_path, track, i, data.tracks.size());
        const WindData* wp = wind.valid ? &wind : nullptr;

        // Estimated power (needed before the hills table for its power column)
        PowerAnalysis pa = reader.estimate_power(power, i, wp);

        // Hill table (annotated with per-climb average power)
        std::vector<Hill> hills = reader.detect_hills(i);
        reader.attach_climb_power(hills, pa);
        io::print_hills(hills);

        // Estimated power summary
        io::print_power_stats(pa.stats, power);

        // Optional time-vs-power CSV (suffix with track index when >1 track)
        if (!power_csv.empty()) {
            std::string out_path = (data.tracks.size() > 1)
                                 ? power_csv + "." + std::to_string(i)
                                 : power_csv;
            if (io::write_power_csv(out_path, track, pa))
                std::cout << "Wrote time-vs-power CSV: " << out_path << "\n\n";
            else
                std::cerr << "Error: could not write CSV to " << out_path << "\n";
        }

        // Optional primitive XY data table (suffix with track index when >1 track)
        if (!xy_path.empty()) {
            std::string out_path = (data.tracks.size() > 1)
                                 ? xy_path + "." + std::to_string(i)
                                 : xy_path;
            if (io::write_xy_file(out_path, track, stats, pa))
                std::cout << "Wrote XY data table: " << out_path << "\n\n";
            else
                std::cerr << "Error: could not write XY table to " << out_path << "\n";
        }

        // Optional mean-maximal power curve (suffix with track index when >1 track)
        if (!power_curve_path.empty()) {
            static const std::vector<Long> kCurveDurations = {
                1, 5, 10, 30, 60, 300, 600, 1200, 1800, 3600, 5400};
            PowerCurve curve = reader.power_curve(pa, kCurveDurations, i);
            std::string out_path = (data.tracks.size() > 1)
                                 ? power_curve_path + "." + std::to_string(i)
                                 : power_curve_path;
            if (!curve.valid)
                std::cerr << "Power curve: no usable power/time data — skipping "
                          << out_path << "\n";
            else if (io::write_power_curve_file(out_path, curve))
                std::cout << "Wrote power curve: " << out_path << "\n\n";
            else
                std::cerr << "Error: could not write power curve to " << out_path << "\n";
        }

        // Optional power histogram (suffix with track index when >1 track)
        if (!power_hist_path.empty()) {
            PowerHistogram hist = reader.power_histogram(pa, hist_bin_w, i);
            std::string out_path = (data.tracks.size() > 1)
                                 ? power_hist_path + "." + std::to_string(i)
                                 : power_hist_path;
            if (!hist.valid)
                std::cerr << "Power histogram: no usable power/time data — skipping "
                          << out_path << "\n";
            else if (io::write_power_hist_file(out_path, hist))
                std::cout << "Wrote power histogram: " << out_path << "\n\n";
            else
                std::cerr << "Error: could not write power histogram to " << out_path << "\n";
        }

        // Optional autocorrelation + power spectrum: one flag per quantity,
        // each writing its own 4-column file. Only the requested channels run.
        const std::vector<std::pair<std::string, std::string>> acf_requests = {
            {"velocity",       acf_velocity},
            {"power",          acf_power},
            {"power_measured", acf_power_measured},
            {"hr",             acf_hr},
            {"cadence",        acf_cadence},
        };
        const Bool any_acf = std::any_of(
            acf_requests.begin(), acf_requests.end(),
            [](const auto& r) { return !r.second.empty(); });

        if (any_acf) {
            std::vector<Channel> channels = extract_channels(track, stats, pa);
            for (const auto& [name, req] : acf_requests) {
                if (req.empty()) continue;                 // quantity not asked for

                // Suffix with the track index when the file has several tracks.
                const std::string out_path = (data.tracks.size() > 1)
                                           ? req + "." + std::to_string(i) : req;

                // Locate the extracted channel; absent means the track lacks it.
                const Channel* ch = nullptr;
                for (const Channel& c : channels)
                    if (c.name == name) { ch = &c; break; }
                if (!ch) {
                    std::cerr << "ACF/PSD: '" << name
                              << "' not present in this track — skipping "
                              << out_path << "\n";
                    continue;
                }

                SpectralResult sr = compute_acf_psd(ch->t_s, ch->value, acf_dt);
                if (!sr.valid)
                    std::cerr << "ACF/PSD: not enough / constant data for " << name
                              << " — skipping " << out_path << "\n";
                else if (io::write_spectral_file(out_path, ch->name, ch->unit, sr))
                    std::cout << "Wrote autocorrelation/spectrum: " << out_path
                              << " (dt=" << sr.dt_s << "s, " << sr.n_samples
                              << " samples)\n";
                else
                    std::cerr << "Error: could not write " << out_path << "\n";
            }
            std::cout << "\n";
        }

        // Fastest distance-based segments
        for (Real d_km : dist_windows) {
            std::ostringstream lbl;
            lbl << std::fixed << std::setprecision(2) << d_km << " km";
            BestSegment seg = reader.fastest_by_distance(d_km * 1000.0, i);
            io::print_best_segment(seg, lbl.str());
        }

        // Fastest time-based segments
        for (Long t_s : time_windows) {
            BestSegment seg = reader.fastest_by_time(t_s, i);
            io::print_best_segment(seg, io::format_duration(t_s));
        }
    }

    return EXIT_SUCCESS;
}
