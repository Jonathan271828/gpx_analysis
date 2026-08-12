#include "arg_parser.hpp"
#include "gpx_reader.hpp"
#include "signal.hpp"
#include "wind.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Bring the wind-source mode enum (defined by the argument parser) into scope
// so obtain_wind() and main() can name it unqualified.
using arg_parser::WindMode;

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

/// Format duration in seconds as "Xh Ym Zs"
static std::string format_duration(Long seconds) {
    Long h = seconds / 3600;
    Long m = (seconds % 3600) / 60;
    Long s = seconds % 60;
    std::ostringstream oss;
    if (h > 0) oss << h << "h ";
    if (h > 0 || m > 0) oss << m << "m ";
    oss << s << "s";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Print track points
// ---------------------------------------------------------------------------

static void print_track_points(const Track& track, Size max_print) {
    const auto& pts = track.points;
    const Size total = pts.size();
    const Size show  = std::min(total, max_print);

    std::cout << "\n--- Track points (showing first " << show
              << " of " << total << ") ---\n";

    for (Size i = 0; i < show; ++i) {
        const auto& p = pts[i];
        std::cout << std::fixed
                  << "[" << std::setw(5) << (i + 1) << "] "
                  << "lat=" << std::setprecision(6) << std::setw(13) << p.lat
                  << "  lon=" << std::setprecision(6) << std::setw(14) << p.lon
                  << "  ele=" << std::setprecision(2) << std::setw(8) << p.ele << " m"
                  << "  time=" << p.time;
        if (p.has_atemp) {
            std::cout << "  temp=" << std::setprecision(1) << p.atemp << " C";
        }
        if (p.has_hr)    std::cout << "  hr=" << p.hr << " bpm";
        if (p.has_cad)   std::cout << "  cad=" << p.cad << " rpm";
        if (p.has_power) std::cout << "  pw=" << p.power << " W";
        std::cout << "\n";
    }

    if (total > show) {
        std::cout << "  ... (" << (total - show) << " more points not shown)\n";
    }
}

// ---------------------------------------------------------------------------
// Print statistics
// ---------------------------------------------------------------------------

static void print_stats(const Track& track, const TrackStats& s) {
    std::cout << "\n=== Statistics for track: \"" << track.name << "\" ===\n";
    if (!track.type.empty())
        std::cout << "  Type           : " << track.type << "\n";
    std::cout << "  Points         : " << s.num_points << "\n";
    std::cout << std::fixed;
    std::cout << "  Total distance : " << std::setprecision(2)
              << s.total_distance_m / 1000.0 << " km\n";
    std::cout << "  Elevation gain : " << std::setprecision(1)
              << s.elevation_gain_m << " m\n";
    std::cout << "  Elevation loss : " << std::setprecision(1)
              << s.elevation_loss_m << " m\n";
    std::cout << "  Min elevation  : " << std::setprecision(1)
              << s.min_ele_m << " m\n";
    std::cout << "  Max elevation  : " << std::setprecision(1)
              << s.max_ele_m << " m\n";
    if (s.duration_s > 0) {
        std::cout << "  Duration       : " << format_duration(s.duration_s) << "\n";
        std::cout << "  Avg speed      : " << std::setprecision(1)
                  << s.avg_speed_kmh << " km/h\n";
    }
    if (s.has_atemp) {
        std::cout << "  Avg temperature: " << std::setprecision(1)
                  << s.avg_atemp << " C\n";
    }
    if (s.has_hr) {
        std::cout << "  Avg heart rate : " << std::setprecision(0)
                  << s.avg_hr << " bpm (min " << s.min_hr
                  << ", max " << s.max_hr << ")\n";
    }
    if (s.has_cad) {
        std::cout << "  Avg cadence    : " << std::setprecision(0)
                  << s.avg_cad << " rpm (min " << s.min_cad
                  << ", max " << s.max_cad << ")\n";
    }
    if (s.has_power) {
        std::cout << "  Avg power      : " << std::setprecision(0)
                  << s.avg_power << " W (min " << s.min_power
                  << ", max " << s.max_power << ")\n";
    }
    std::cout << "  Avg climb grade: +" << std::setprecision(1)
              << s.avg_climb_pct << " %\n";
    std::cout << "  Avg desc grade : " << std::setprecision(1)
              << s.avg_descent_pct << " %\n";
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Print hill table
// ---------------------------------------------------------------------------

static void print_hills(const std::vector<Hill>& hills) {
    std::cout << "\n--- Hills (min grade: 1%, min gain: 10 m, gap tolerance: 20 m) ---\n";

    if (hills.empty()) {
        std::cout << "  No hills detected.\n\n";
        return;
    }

    // Header
    std::cout << std::fixed
              << " " << std::setw(3) << "#"
              << "  " << std::setw(10) << "Distance"
              << "  " << std::setw(8)  << "Gain"
              << "  " << std::setw(11) << "Avg grade"
              << "  " << std::setw(10) << "Start ele"
              << "  " << std::setw(10) << "End ele"
              << "  " << std::setw(10) << "Avg power"
              << "  " << "Start time"
              << "\n";
    std::cout << " " << std::string(3,  '-')
              << "  " << std::string(10, '-')
              << "  " << std::string(8,  '-')
              << "  " << std::string(11, '-')
              << "  " << std::string(10, '-')
              << "  " << std::string(10, '-')
              << "  " << std::string(10, '-')
              << "  " << std::string(24, '-')
              << "\n";

    for (Size i = 0; i < hills.size(); ++i) {
        const auto& h = hills[i];
        std::cout << " " << std::setw(3) << (i + 1)
                  << "  " << std::setprecision(2) << std::setw(8)
                  << h.distance_m / 1000.0 << " km"
                  << "  " << std::setprecision(1) << std::setw(6)
                  << h.gain_m << " m"
                  << "  +" << std::setprecision(1) << std::setw(7)
                  << h.avg_grade_pct << " %"
                  << "  " << std::setprecision(1) << std::setw(8)
                  << h.start_ele_m << " m"
                  << "  " << std::setprecision(1) << std::setw(8)
                  << h.end_ele_m << " m";
        if (h.has_power)
            std::cout << "  " << std::setprecision(0) << std::setw(8)
                      << h.avg_power_w << " W";
        else
            std::cout << "  " << std::setw(8) << "-" << "  ";
        std::cout << "  " << h.start_time
                  << "\n";
    }

    std::cout << "\nTotal: " << hills.size() << " hill"
              << (hills.size() != 1 ? "s" : "") << "\n\n";
}

// ---------------------------------------------------------------------------
// Print best segment result
// ---------------------------------------------------------------------------

static void print_best_segment(const BestSegment&  seg,
                                const std::string&  label)
{
    std::cout << "\n=== Fastest " << label << " segment ===\n";
    if (!seg.valid) {
        std::cout << "  (window is larger than the track — no result)\n\n";
        return;
    }
    std::cout << std::fixed;
    std::cout << "  Avg speed  : " << std::setprecision(1)
              << seg.avg_speed_kmh << " km/h\n";
    std::cout << "  Distance   : " << std::setprecision(3)
              << seg.distance_m / 1000.0 << " km\n";
    std::cout << "  Duration   : " << format_duration(seg.duration_s) << "\n";
    std::cout << "  Start      : " << seg.start_time
              << "  (lat=" << std::setprecision(6) << seg.start_lat
              << ", lon=" << seg.start_lon << ")\n";
    std::cout << "  End        : " << seg.end_time
              << "  (lat=" << std::setprecision(6) << seg.end_lat
              << ", lon=" << seg.end_lon << ")\n";
    std::cout << "  Point idx  : " << seg.start_idx
              << " -> " << seg.end_idx << "\n\n";
}

// ---------------------------------------------------------------------------
// Print estimated power summary
// ---------------------------------------------------------------------------

static void print_power_stats(const PowerStats& ps, const PowerParams& pp) {
    std::cout << "\n=== Estimated power ===\n";
    if (!ps.valid) {
        std::cout << "  (insufficient data to estimate power)\n\n";
        return;
    }
    std::cout << std::fixed;
    std::cout << "  Model          : mass " << std::setprecision(1) << pp.total_mass_kg
              << " kg, Crr " << std::setprecision(4) << pp.crr
              << ", CdA " << std::setprecision(3) << pp.cda << " m^2"
              << ", drivetrain " << std::setprecision(1) << (pp.drivetrain_eff * 100.0)
              << " %\n";
    std::cout << "  Avg est. power : " << std::setprecision(0) << ps.avg_power_w << " W\n";
    std::cout << "  Max est. power : " << std::setprecision(0) << ps.max_power_w << " W\n";
    std::cout << "  Work done      : " << std::setprecision(0) << ps.total_kj << " kJ\n";
    if (ps.has_wind) {
        std::cout << "  Wind           : Open-Meteo, avg headwind "
                  << std::setprecision(1) << ps.avg_headwind_ms << " m/s"
                  << " (+head / -tail)\n";
    }
    if (ps.has_measured) {
        std::cout << "  Measured avg   : " << std::setprecision(0) << ps.avg_measured_w << " W\n";
        std::cout << "  Mean abs error : " << std::setprecision(0) << ps.mean_abs_err_w << " W\n";
        std::cout << "  Mean bias      : " << std::setprecision(1) << ps.mean_bias_w
                  << " W (est - measured)\n";
    }
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Write time-vs-power CSV
// ---------------------------------------------------------------------------

static Bool write_power_csv(const std::string&   path,
                            const Track&          track,
                            const PowerAnalysis&  pa)
{
    std::ofstream out(path);
    if (!out) return false;

    const auto& pts = track.points;
    const Bool has_meas = pa.stats.has_measured;
    const Bool has_wind = pa.stats.has_wind;

    out << "time,elapsed_s,est_power_w";
    if (has_meas) out << ",measured_power_w";
    if (has_wind) out << ",headwind_ms";
    out << "\n";

    out << std::fixed << std::setprecision(1);
    for (Size i = 0; i < pts.size(); ++i) {
        out << pts[i].time << ',' << pa.t_offset_s[i] << ',' << pa.point_power_w[i];
        if (has_meas) {
            out << ',';
            if (pts[i].has_power) out << pts[i].power;   // measured, may be blank
        }
        if (has_wind) out << ',' << pa.headwind_ms[i];
        out << '\n';
    }
    return true;
}

// ---------------------------------------------------------------------------
// Write a primitive XY data table (whitespace-separated, #-commented header)
//
// One row per track point, columns in the order requested:
//   time (s), distance (km), velocity (km/h), heart rate, power (W),
//   temperature, then everything else we carry. Sensor columns
//   (hr/power/temp/cadence/headwind) are only
//   emitted when the track actually contains that field; per-point gaps within
//   an emitted column are written as NaN so the table stays rectangular and
//   loads cleanly in gnuplot/numpy.
// ---------------------------------------------------------------------------

static Bool write_xy_file(const std::string&   path,
                          const Track&          track,
                          const TrackStats&     stats,
                          const PowerAnalysis&  pa)
{
    std::ofstream out(path);
    if (!out) return false;

    const auto& pts      = track.points;
    const Bool  has_hr   = stats.has_hr;
    const Bool  has_pw   = stats.has_power;    // measured power
    const Bool  has_temp = stats.has_atemp;
    const Bool  has_cad  = stats.has_cad;
    const Bool  has_wind = pa.stats.has_wind;
    const Bool  has_est  = pa.stats.valid;     // estimated power series

    // Commented header: column index + name/unit, one line, starts with '#'.
    out << "# GPXAna per-point track data\n";
    if (!track.name.empty()) out << "# track: " << track.name << "\n";
    out << "#";
    Int col = 1;
    out << " " << col++ << ":elapsed_s"
        << " " << col++ << ":distance_km"
        << " " << col++ << ":velocity_kmh";
    if (has_hr)   out << " " << col++ << ":hr_bpm";
    if (has_pw)   out << " " << col++ << ":power_w";
    if (has_temp) out << " " << col++ << ":temp_C";
    if (has_cad)  out << " " << col++ << ":cadence_rpm";
    out << " " << col++ << ":elevation_m"
        << " " << col++ << ":lat"
        << " " << col++ << ":lon";
    if (has_est)  out << " " << col++ << ":est_power_w";
    if (has_wind) out << " " << col++ << ":headwind_ms";
    out << "\n";

    for (Size i = 0; i < pts.size(); ++i) {
        const auto& p = pts[i];
        std::ostringstream row;
        row << std::fixed;

        // time (elapsed seconds; NaN if the timestamp was unparseable)
        if (pa.t_offset_s[i] >= 0) row << pa.t_offset_s[i];
        else                       row << "NaN";

        // distance (km) and velocity (km/h)
        row << ' ' << std::setprecision(3) << pa.cum_dist_m[i] / 1000.0;
        if (i == 0) row << ' ' << "NaN";                 // no step into point 0
        else        row << ' ' << std::setprecision(2) << pa.speed_ms[i] * 3.6;

        if (has_hr)   row << ' ' << (p.has_hr    ? std::to_string(p.hr)  : "NaN");
        if (has_pw)   row << ' ' << (p.has_power ? std::to_string(p.power): "NaN");
        if (has_temp) {
            row << ' ';
            if (p.has_atemp) row << std::setprecision(1) << p.atemp;
            else             row << "NaN";
        }
        if (has_cad)  row << ' ' << (p.has_cad   ? std::to_string(p.cad) : "NaN");

        row << ' ' << std::setprecision(2) << p.ele
            << ' ' << std::setprecision(6) << p.lat
            << ' ' << std::setprecision(6) << p.lon;

        if (has_est)  row << ' ' << std::setprecision(1) << pa.point_power_w[i];
        if (has_wind) row << ' ' << std::setprecision(2) << pa.headwind_ms[i];

        out << row.str() << '\n';
    }
    return true;
}

// ---------------------------------------------------------------------------
// Write the mean-maximal power curve as an XY table (duration vs watts)
// ---------------------------------------------------------------------------

static Bool write_power_curve_file(const std::string& path,
                                   const PowerCurve&  curve)
{
    std::ofstream out(path);
    if (!out) return false;

    out << "# GPXAna mean-maximal power curve (best average power per duration)\n";
    out << "# 1:duration_s 2:est_power_w";
    if (curve.has_measured) out << " 3:measured_power_w";
    out << "\n";

    out << std::fixed << std::setprecision(1);
    for (Size i = 0; i < curve.duration_s.size(); ++i) {
        out << curve.duration_s[i] << ' ' << curve.est_power_w[i];
        if (curve.has_measured) out << ' ' << curve.meas_power_w[i];
        out << '\n';
    }
    return true;
}

// ---------------------------------------------------------------------------
// Write the power histogram as an XY table (power band vs seconds in band)
// ---------------------------------------------------------------------------

static Bool write_power_hist_file(const std::string&    path,
                                  const PowerHistogram& hist)
{
    std::ofstream out(path);
    if (!out) return false;

    out << "# GPXAna power histogram (time in each " << std::fixed
        << std::setprecision(0) << hist.bin_w << " W band)\n";
    out << "# 1:power_w_lo 2:est_seconds";
    if (hist.has_measured) out << " 3:measured_seconds";
    out << "\n";

    out << std::fixed << std::setprecision(1);
    for (Size b = 0; b < hist.bin_lo_w.size(); ++b) {
        out << hist.bin_lo_w[b] << ' ' << hist.est_seconds[b];
        if (hist.has_measured) out << ' ' << hist.meas_seconds[b];
        out << '\n';
    }
    return true;
}

// ---------------------------------------------------------------------------
// Write a channel's autocorrelation + power spectrum as a 4-column table
//
//   col 1: time lag (s)      col 2: normalized autocorrelation
//   col 3: frequency (Hz)    col 4: power spectral density
//
// The time-domain (lag/acf) and frequency-domain (freq/psd) pairs have
// different natural lengths; the shorter is NaN-padded so the file stays
// rectangular and loads cleanly in gnuplot/numpy.
// ---------------------------------------------------------------------------

static Bool write_spectral_file(const std::string&    path,
                                const std::string&    channel,
                                const std::string&    unit,
                                const SpectralResult& sr)
{
    std::ofstream out(path);
    if (!out) return false;

    out << "# GPXAna autocorrelation & power spectrum: " << channel
        << " (" << unit << ")\n";
    out << "# resampled dt = " << sr.dt_s << " s, samples = " << sr.n_samples
        << "\n";
    out << "# 1:lag_s  2:autocorr  3:freq_hz  4:psd[" << unit << "^2/Hz]\n";

    out.precision(8);   // general format: fixed for small, scientific for large
    const Size rows = sr.freq_hz.size();  // == acf/lag length (padded)
    for (Size i = 0; i < rows; ++i) {
        out << sr.lag_s[i] << ' ' << sr.acf[i] << ' '
            << sr.freq_hz[i] << ' ' << sr.psd[i] << '\n';
    }
    return true;
}

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
        print_track_points(track, max_print);

        // Overall statistics
        TrackStats stats = reader.compute_stats(i);
        print_stats(track, stats);

        // Wind data (fetched/loaded per track), then estimated power
        WindData wind = obtain_wind(wind_mode, wind_path, track, i, data.tracks.size());
        const WindData* wp = wind.valid ? &wind : nullptr;

        // Estimated power (needed before the hills table for its power column)
        PowerAnalysis pa = reader.estimate_power(power, i, wp);

        // Hill table (annotated with per-climb average power)
        std::vector<Hill> hills = reader.detect_hills(i);
        reader.attach_climb_power(hills, pa);
        print_hills(hills);

        // Estimated power summary
        print_power_stats(pa.stats, power);

        // Optional time-vs-power CSV (suffix with track index when >1 track)
        if (!power_csv.empty()) {
            std::string out_path = (data.tracks.size() > 1)
                                 ? power_csv + "." + std::to_string(i)
                                 : power_csv;
            if (write_power_csv(out_path, track, pa))
                std::cout << "Wrote time-vs-power CSV: " << out_path << "\n\n";
            else
                std::cerr << "Error: could not write CSV to " << out_path << "\n";
        }

        // Optional primitive XY data table (suffix with track index when >1 track)
        if (!xy_path.empty()) {
            std::string out_path = (data.tracks.size() > 1)
                                 ? xy_path + "." + std::to_string(i)
                                 : xy_path;
            if (write_xy_file(out_path, track, stats, pa))
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
            else if (write_power_curve_file(out_path, curve))
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
            else if (write_power_hist_file(out_path, hist))
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
                else if (write_spectral_file(out_path, ch->name, ch->unit, sr))
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
            print_best_segment(seg, lbl.str());
        }

        // Fastest time-based segments
        for (Long t_s : time_windows) {
            BestSegment seg = reader.fastest_by_time(t_s, i);
            print_best_segment(seg, format_duration(t_s));
        }
    }

    return EXIT_SUCCESS;
}
