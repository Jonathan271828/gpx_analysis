#include "screen_output.hpp"

#include "io_base.hpp"     // format_duration

#include <algorithm>       // std::min
#include <iomanip>
#include <iostream>
#include <string>

namespace io {

// ---------------------------------------------------------------------------
// Print track points
// ---------------------------------------------------------------------------

void print_track_points(const Track& track, Size max_print) {
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

void print_stats(const Track& track, const TrackStats& s) {
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

void print_hills(const std::vector<Hill>& hills) {
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

void print_best_segment(const BestSegment& seg, const std::string& label)
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

void print_power_stats(const PowerStats& ps, const PowerParams& pp) {
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

} // namespace io
