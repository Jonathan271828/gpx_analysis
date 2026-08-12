#include "file_output.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace io {

// ---------------------------------------------------------------------------
// Write time-vs-power CSV
// ---------------------------------------------------------------------------

Bool write_power_csv(const std::string&   path,
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

Bool write_xy_file(const std::string&   path,
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

Bool write_power_curve_file(const std::string& path,
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

Bool write_power_hist_file(const std::string&    path,
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

Bool write_spectral_file(const std::string&    path,
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

} // namespace io
