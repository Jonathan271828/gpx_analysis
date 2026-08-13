#include "screen_output.hpp"

#include "io_base.hpp"     // format_duration

#include <algorithm>       // std::min
#include <iomanip>
#include <iostream>
#include <sstream>
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
              << "  " << std::setw(10) << "Avg power"
              << "  " << std::setw(9)  << "VAM"
              << "  " << std::setw(3)  << "Cat"
              << "  " << "Start time"
              << "\n";
    std::cout << " " << std::string(3,  '-')
              << "  " << std::string(10, '-')
              << "  " << std::string(8,  '-')
              << "  " << std::string(11, '-')
              << "  " << std::string(10, '-')
              << "  " << std::string(9,  '-')
              << "  " << std::string(3,  '-')
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
                  << h.avg_grade_pct << " %";
        if (h.has_power)
            std::cout << "  " << std::setprecision(0) << std::setw(8)
                      << h.avg_power_w << " W";
        else
            std::cout << "  " << std::setw(8) << "-" << "  ";
        std::cout << "  " << std::setprecision(0) << std::setw(6) << h.vam_mh << " m/h"
                  << "  " << std::setw(3) << (h.category.empty() ? "-" : h.category)
                  << "  " << h.start_time
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

void print_power_stats(const PowerStats& ps, const PowerParams& pp, Real body_mass_kg) {
    std::cout << "\n=== Estimated power ===\n";
    if (!ps.valid) {
        std::cout << "  (insufficient data to estimate power)\n\n";
        return;
    }
    const Bool wkg = body_mass_kg > 0.0;
    std::cout << std::fixed;
    std::cout << "  Model          : mass " << std::setprecision(1) << pp.total_mass_kg
              << " kg, Crr " << std::setprecision(4) << pp.crr
              << ", CdA " << std::setprecision(3) << pp.cda << " m^2"
              << ", drivetrain " << std::setprecision(1) << (pp.drivetrain_eff * 100.0)
              << " %\n";
    std::cout << "  Avg est. power : " << std::setprecision(0) << ps.avg_power_w << " W";
    if (wkg) std::cout << " (" << std::setprecision(2) << ps.avg_power_w / body_mass_kg << " W/kg)";
    std::cout << "\n";
    std::cout << "  Max est. power : " << std::setprecision(0) << ps.max_power_w << " W";
    if (wkg) std::cout << " (" << std::setprecision(2) << ps.max_power_w / body_mass_kg << " W/kg)";
    std::cout << "\n";
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
// Print training load (NP / IF / TSS / VI / energy / W-per-kg)
// ---------------------------------------------------------------------------

void print_training_load(const metrics::TrainingLoad& t) {
    std::cout << "\n=== Training load ===\n";
    if (!t.valid) { std::cout << "  (insufficient power data)\n\n"; return; }
    std::cout << std::fixed;
    std::cout << "  Source         : "
              << (t.from_measured ? "measured power" : "estimated power") << "\n";
    std::cout << "  FTP            : " << std::setprecision(0) << t.ftp_w << " W\n";
    std::cout << "  Avg power      : " << std::setprecision(0) << t.avg_power_w
              << " W (" << std::setprecision(2) << t.avg_wkg << " W/kg)\n";
    std::cout << "  Normalized pwr : " << std::setprecision(0) << t.np_w
              << " W (" << std::setprecision(2) << t.np_wkg << " W/kg)\n";
    std::cout << "  Intensity (IF) : " << std::setprecision(2) << t.if_ << "\n";
    std::cout << "  TSS            : " << std::setprecision(0) << t.tss << "\n";
    std::cout << "  Variability    : " << std::setprecision(2) << t.vi << "\n";
    std::cout << "  Work / energy  : " << std::setprecision(0) << t.kj << " kJ (~"
              << t.kcal << " kcal)\n";
    if (t.has_hr)
        std::cout << "  Efficiency (EF): " << std::setprecision(2) << t.ef
                  << " (NP/avg HR " << std::setprecision(0) << t.avg_hr << " bpm)\n";
    std::cout << "  Moving time    : " << format_duration(t.moving_s) << "\n\n";
}

// ---------------------------------------------------------------------------
// Print aerobic decoupling
// ---------------------------------------------------------------------------

void print_decoupling(const metrics::Decoupling& d) {
    if (!d.valid) return;   // no heart-rate data — nothing to show
    std::cout << "=== Aerobic decoupling (Pw:Hr) ===\n" << std::fixed
              << std::setprecision(3)
              << "  1st-half ratio : " << d.first_ratio << " W/bpm\n"
              << "  2nd-half ratio : " << d.second_ratio << " W/bpm\n"
              << std::setprecision(1)
              << "  Decoupling     : " << d.pct << " %"
              << (d.pct > 5.0 ? "  (>5%: aerobic drift / fatigue)"
                              : "  (<=5%: good durability)")
              << "\n\n";
}

// ---------------------------------------------------------------------------
// Print a zone-distribution table (power or heart rate)
// ---------------------------------------------------------------------------

void print_zone_table(const zones::ZoneTable& z) {
    if (!z.valid) return;
    std::cout << "=== Time in " << z.kind << " zones (" << z.basis << ") ===\n"
              << std::fixed;
    for (const zones::Zone& zn : z.zones) {
        const Real pct = z.total_s > 0.0 ? 100.0 * zn.seconds / z.total_s : 0.0;
        std::ostringstream rng;
        rng << std::fixed << std::setprecision(0);
        if (zn.hi < 0.0) rng << zn.lo << "+";
        else             rng << zn.lo << "-" << zn.hi;
        std::cout << "  " << std::left << std::setw(20) << zn.label << std::right
                  << std::setw(12) << rng.str()
                  << "  " << std::setw(9) << format_duration(static_cast<Long>(zn.seconds))
                  << "  " << std::setprecision(1) << std::setw(5) << pct << " %\n";
    }
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Print the distance-split table
// ---------------------------------------------------------------------------

void print_splits(const std::vector<splits::Split>& sp) {
    if (sp.empty()) return;
    std::cout << "=== Splits ===\n" << std::fixed
              << " " << std::setw(7) << "Split"
              << "  " << std::setw(8) << "Dist"
              << "  " << std::setw(9) << "Time"
              << "  " << std::setw(8) << "Speed"
              << "  " << std::setw(7) << "Power"
              << "  " << std::setw(4) << "HR"
              << "  " << std::setw(7) << "Climb"
              << "\n";
    for (const splits::Split& s : sp) {
        std::cout << " " << std::setprecision(0) << std::setw(5) << s.start_km << "km"
                  << "  " << std::setprecision(2) << std::setw(6) << s.dist_m / 1000.0 << "km"
                  << "  " << std::setw(9) << format_duration(s.dur_s)
                  << "  " << std::setprecision(1) << std::setw(5) << s.avg_speed_kmh << "kmh"
                  << "  ";
        if (s.has_power) std::cout << std::setprecision(0) << std::setw(5) << s.avg_power_w << " W";
        else             std::cout << std::setw(7) << "-";
        std::cout << "  ";
        if (s.has_hr) std::cout << std::setprecision(0) << std::setw(4) << s.avg_hr;
        else          std::cout << std::setw(4) << "-";
        std::cout << "  " << std::setprecision(0) << std::setw(5) << s.gain_m << " m\n";
    }
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Print the critical-power fit
// ---------------------------------------------------------------------------

void print_cp(const cp::CpFit& f) {
    if (!f.valid) return;
    std::cout << "=== Critical power model ===\n" << std::fixed
              << "  Source         : " << (f.measured ? "measured" : "estimated")
              << " power curve\n"
              << "  CP (~FTP)      : " << std::setprecision(0) << f.cp_w << " W\n"
              << "  W' (anaerobic) : " << std::setprecision(0) << f.w_prime_j << " J ("
              << std::setprecision(1) << f.w_prime_j / 1000.0 << " kJ)\n"
              << "  Fit efforts    : " << f.n_points << " (2-20 min)\n\n";
}

// ---------------------------------------------------------------------------
// Print the W'-match summary
// ---------------------------------------------------------------------------

void print_matches(const cp::MatchStats& m) {
    if (!m.valid) return;
    std::cout << "=== Anaerobic reserve (W' matches) ===\n" << std::fixed
              << "  Matches burned : " << m.matches
              << "  (deep W' expenditures)\n"
              << "  Lowest W'bal   : " << std::setprecision(0) << m.min_j << " J ("
              << std::setprecision(0) << m.min_pct << " %)\n"
              << "  W'bal at end   : " << std::setprecision(0) << m.end_j << " J ("
              << std::setprecision(0) << m.end_pct << " %)\n\n";
}

// ---------------------------------------------------------------------------
// Print the peak-power efforts table
// ---------------------------------------------------------------------------

void print_peaks(const std::vector<peaks::PeakEffort>& pk) {
    if (pk.empty()) return;
    std::cout << "=== Peak power efforts"
              << (pk.front().measured ? " (measured)" : " (estimated)") << " ===\n"
              << std::fixed
              << "  " << std::setw(9) << "Duration"
              << "  " << std::setw(7) << "Power"
              << "  " << "Starting at\n";
    for (const peaks::PeakEffort& e : pk) {
        std::cout << "  " << std::setw(9) << format_duration(e.duration_s)
                  << "  " << std::setprecision(0) << std::setw(5) << e.avg_power_w << " W"
                  << "  " << format_duration(e.start_offset_s) << " (" << e.start_time << ")\n";
    }
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Print the quadrant analysis (force vs cadence)
// ---------------------------------------------------------------------------

void print_quadrant(const quadrant::Quadrants& q) {
    if (!q.valid) return;
    const char* labels[4] = {
        "Q1 high force / high cadence",
        "Q2 high force / low cadence ",
        "Q3 low force  / low cadence ",
        "Q4 low force  / high cadence"};
    std::cout << "=== Quadrant analysis ===\n" << std::fixed
              << "  Crosshair      : " << std::setprecision(0) << q.aepf_threshold_n
              << " N, " << std::setprecision(2) << q.cpv_threshold_ms << " m/s"
              << " (FTP @ 90 rpm); avg cadence " << std::setprecision(0)
              << q.avg_cadence_rpm << " rpm\n";
    for (int k = 0; k < 4; ++k) {
        const Real pct = q.total_s > 0.0 ? 100.0 * q.seconds[k] / q.total_s : 0.0;
        std::cout << "  " << labels[k]
                  << "  " << std::setw(9) << format_duration(static_cast<Long>(q.seconds[k]))
                  << "  " << std::setprecision(1) << std::setw(5) << pct << " %\n";
    }
    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Print the multi-ride training trend (CTL / ATL / TSB)
// ---------------------------------------------------------------------------

void print_trends(const std::vector<trends::TrendPoint>& tp) {
    if (tp.empty()) return;
    std::cout << "\n=== Training trend across " << tp.size() << " ride"
              << (tp.size() != 1 ? "s" : "") << " ===\n" << std::fixed
              << "  " << std::left << std::setw(12) << "Date" << std::right
              << std::setw(7) << "TSS" << std::setw(8) << "CTL"
              << std::setw(8) << "ATL" << std::setw(8) << "TSB"
              << "  " << "Ride\n";
    for (const trends::TrendPoint& p : tp) {
        std::cout << "  " << std::left << std::setw(12) << p.date << std::right
                  << std::setprecision(0) << std::setw(7) << p.tss
                  << std::setprecision(1) << std::setw(8) << p.ctl
                  << std::setw(8) << p.atl << std::setw(8) << p.tsb
                  << "  " << p.label << "\n";
    }
    std::cout << "  CTL=fitness, ATL=fatigue, TSB=form (CTL-ATL, entering the day)\n\n";
}

// ---------------------------------------------------------------------------
// print_durability — the power-duration curve as work accumulates
// ---------------------------------------------------------------------------

void print_durability(const durability::Report& r) {
    if (!r.valid) return;

    std::cout << "=== Fatigue resistance"
              << (r.measured ? " (measured)" : " (estimated)")
              << ", over " << std::fixed << std::setprecision(0) << r.total_kj
              << " kJ ===\n"
              << "  Best power for each duration, starting only after this much work:\n"
              << "  " << std::setw(9) << "Duration";
    for (const Real kj : r.thresholds_kj) {
        std::ostringstream head;
        head << std::fixed << std::setprecision(0) << kj << " kJ";
        std::cout << "  " << std::setw(8) << head.str();
    }
    std::cout << "  " << std::setw(7) << "Fade\n";

    for (const durability::Curve& c : r.curves) {
        std::cout << "  " << std::setw(9) << format_duration(c.duration_s);
        for (const durability::Effort& e : c.efforts) {
            std::ostringstream cell;
            if (e.found) cell << std::fixed << std::setprecision(0) << e.avg_power_w << " W";
            else         cell << "-";
            std::cout << "  " << std::setw(8) << cell.str();
        }
        std::ostringstream fade;
        if (c.valid) fade << std::fixed << std::setprecision(1) << std::showpos
                          << c.fade_pct << " %";
        else         fade << "-";
        std::cout << "  " << std::setw(7) << fade.str() << "\n";
    }
    std::cout << "  Fade is the drop from the freshest column to the deepest one "
                 "reached;\n"
                 "  a large fade at long durations is the signature that matters.\n\n";
}
} // namespace io
