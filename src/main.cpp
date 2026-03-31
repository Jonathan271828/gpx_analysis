#include "gpx_reader.h"

#include <cstdlib>
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
              << "  " << "Start time"
              << "\n";
    std::cout << " " << std::string(3,  '-')
              << "  " << std::string(10, '-')
              << "  " << std::string(8,  '-')
              << "  " << std::string(11, '-')
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
                  << h.end_ele_m << " m"
                  << "  " << h.start_time
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
// Usage
// ---------------------------------------------------------------------------

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <file.gpx> [options]\n\n"
              << "Options:\n"
              << "  --points N   print first N track points (default: 10)\n"
              << "  --dist  D    find fastest segment of D km  (e.g. --dist 5.0)\n"
              << "  --time  T    find fastest segment of T seconds (e.g. --time 300)\n"
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

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--points" && i + 1 < argc) {
            max_print = static_cast<std::size_t>(std::atoi(argv[++i]));
        } else if (arg == "--dist" && i + 1 < argc) {
            dist_windows.push_back(std::atof(argv[++i]));
        } else if (arg == "--time" && i + 1 < argc) {
            time_windows.push_back(static_cast<long>(std::atol(argv[++i])));
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

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

        // Hill table
        std::vector<Hill> hills = reader.detect_hills(i);
        print_hills(hills);

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
