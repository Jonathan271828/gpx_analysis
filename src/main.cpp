#include "gpx_reader.h"
#include "wind.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Formatting helpers
// ---------------------------------------------------------------------------

/// Format duration in seconds as "Xh Ym Zs"
static std::string format_duration(long seconds) {
    long h = seconds / 3600;
    long m = (seconds % 3600) / 60;
    long s = seconds % 60;
    std::ostringstream oss;
    if (h > 0) oss << h << "h ";
    if (h > 0 || m > 0) oss << m << "m ";
    oss << s << "s";
    return oss.str();
}

// ---------------------------------------------------------------------------
// Print track points
// ---------------------------------------------------------------------------

static void print_track_points(const Track& track, std::size_t max_print) {
    const auto& pts = track.points;
    const std::size_t total = pts.size();
    const std::size_t show  = std::min(total, max_print);

    std::cout << "\n--- Track points (showing first " << show
              << " of " << total << ") ---\n";

    for (std::size_t i = 0; i < show; ++i) {
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

    for (std::size_t i = 0; i < hills.size(); ++i) {
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

static bool write_power_csv(const std::string&   path,
                            const Track&          track,
                            const PowerAnalysis&  pa)
{
    std::ofstream out(path);
    if (!out) return false;

    const auto& pts = track.points;
    const bool has_meas = pa.stats.has_measured;
    const bool has_wind = pa.stats.has_wind;

    out << "time,elapsed_s,est_power_w";
    if (has_meas) out << ",measured_power_w";
    if (has_wind) out << ",headwind_ms";
    out << "\n";

    out << std::fixed << std::setprecision(1);
    for (std::size_t i = 0; i < pts.size(); ++i) {
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
// Obtain wind data for a track (network fetch and/or JSON cache/file)
// ---------------------------------------------------------------------------

enum class WindMode { Off, Fetch, Cache, File };

static WindData obtain_wind(WindMode mode, const std::string& path,
                            const Track& track, std::size_t track_index,
                            std::size_t ntracks)
{
    WindData wind;
    if (mode == WindMode::Off) return wind;

    const auto& pts = track.points;
    if (pts.size() < 2 || pts.front().time.size() < 10 || pts.back().time.size() < 10) {
        std::cerr << "Wind: track has no usable coordinates/timestamps — skipping.\n";
        return wind;
    }

    // Request location = track centroid (ERA5's ~25 km grid makes finer pointless)
    double lat_sum = 0.0, lon_sum = 0.0;
    for (const auto& p : pts) { lat_sum += p.lat; lon_sum += p.lon; }
    const double lat = lat_sum / static_cast<double>(pts.size());
    const double lon = lon_sum / static_cast<double>(pts.size());
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
// Usage
// ---------------------------------------------------------------------------

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <file.gpx> [options]\n\n"
              << "Options:\n"
              << "  --points N       print first N track points (default: 10)\n"
              << "  --dist  D        find fastest segment of D km  (e.g. --dist 5.0)\n"
              << "  --time  T        find fastest segment of T seconds (e.g. --time 300)\n"
              << "\nPower estimation (Strava-style physics model, runs by default):\n"
              << "  --mass  M        total rider+bike mass in kg (default: 80)\n"
              << "  --rider R        rider mass in kg (summed with --bike if --mass unset)\n"
              << "  --bike  B        bike mass in kg (summed with --rider if --mass unset)\n"
              << "  --crr   C        rolling resistance coefficient (default: 0.005)\n"
              << "  --cda   A        aerodynamic drag area CdA in m^2 (default: 0.32)\n"
              << "  --drivetrain E   drivetrain efficiency 0..1 (default: 0.977)\n"
              << "  --power-csv F    write a time-vs-power CSV to file F\n"
              << "\nWind (Open-Meteo historical API; improves the aero term):\n"
              << "  --wind           fetch historical wind and apply it\n"
              << "  --wind-cache F   like --wind, but cache to/read from file F\n"
              << "  --wind-file F    apply wind from local JSON file F (offline)\n"
              << "\nMultiple --dist and --time flags are supported.\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const std::string filepath = argv[1];

    // Parse options
    std::size_t          max_print   = 10;
    std::vector<double>  dist_windows;   // km
    std::vector<long>    time_windows;   // seconds

    // Power estimation parameters (mass is optional; default rider+bike = 80 kg)
    PowerParams power;
    double      rider_kg  = 71.0;
    double      bike_kg   = 9.0;
    bool        mass_set  = false;
    double      mass_total = 80.0;
    std::string power_csv;

    WindMode    wind_mode = WindMode::Off;
    std::string wind_path;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--points" && i + 1 < argc) {
            max_print = static_cast<std::size_t>(std::atoi(argv[++i]));
        } else if (arg == "--dist" && i + 1 < argc) {
            dist_windows.push_back(std::atof(argv[++i]));
        } else if (arg == "--time" && i + 1 < argc) {
            time_windows.push_back(static_cast<long>(std::atol(argv[++i])));
        } else if (arg == "--mass" && i + 1 < argc) {
            mass_total = std::atof(argv[++i]); mass_set = true;
        } else if (arg == "--rider" && i + 1 < argc) {
            rider_kg = std::atof(argv[++i]);
        } else if (arg == "--bike" && i + 1 < argc) {
            bike_kg = std::atof(argv[++i]);
        } else if (arg == "--crr" && i + 1 < argc) {
            power.crr = std::atof(argv[++i]);
        } else if (arg == "--cda" && i + 1 < argc) {
            power.cda = std::atof(argv[++i]);
        } else if (arg == "--drivetrain" && i + 1 < argc) {
            power.drivetrain_eff = std::atof(argv[++i]);
        } else if (arg == "--power-csv" && i + 1 < argc) {
            power_csv = argv[++i];
        } else if (arg == "--wind") {
            wind_mode = WindMode::Fetch;
        } else if (arg == "--wind-cache" && i + 1 < argc) {
            wind_mode = WindMode::Cache; wind_path = argv[++i];
        } else if (arg == "--wind-file" && i + 1 < argc) {
            wind_mode = WindMode::File; wind_path = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    // --mass sets the total directly; otherwise sum rider + bike (default 80 kg)
    power.total_mass_kg = mass_set ? mass_total : (rider_kg + bike_kg);

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

    for (std::size_t i = 0; i < data.tracks.size(); ++i) {
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

        // Fastest distance-based segments
        for (double d_km : dist_windows) {
            std::ostringstream lbl;
            lbl << std::fixed << std::setprecision(2) << d_km << " km";
            BestSegment seg = reader.fastest_by_distance(d_km * 1000.0, i);
            print_best_segment(seg, lbl.str());
        }

        // Fastest time-based segments
        for (long t_s : time_windows) {
            BestSegment seg = reader.fastest_by_time(t_s, i);
            print_best_segment(seg, format_duration(t_s));
        }
    }

    return EXIT_SUCCESS;
}
