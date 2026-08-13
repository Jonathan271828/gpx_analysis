#include "app.hpp"

#include "channels.hpp"
#include "cp_model.hpp"
#include "file_output.hpp"
#include "durability.hpp"
#include "gpx_reader.hpp"
#include "io_base.hpp"
#include "metrics.hpp"
#include "peaks.hpp"
#include "quadrant.hpp"
#include "screen_output.hpp"
#include "signal.hpp"
#include "splits.hpp"
#include "trends.hpp"
#include "wind.hpp"
#include "zones.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace app {

namespace {

// Analyse a single track: print the report and run every requested export.
// `file_tag` disambiguates output filenames when several files/tracks are
// processed in one run. Returns the track's TSS (0 if power was unusable).
Real analyse_track(GpxReader&                 reader,
                   Size                       track_index,
                   const arg_parser::Options& opts,
                   const std::string&         file_tag,
                   Size                       ntracks)
{
    const Track& track = reader.data().tracks[track_index];
    const Size   i     = track_index;

    // Build a per-output filename: base + file tag + track tag.
    auto outp = [&](const std::string& base) {
        std::string s = base + file_tag;
        if (ntracks > 1) s += "." + std::to_string(i);
        return s;
    };

    io::print_track_points(track, opts.max_print);

    TrackStats stats = reader.compute_stats(i);
    io::print_stats(track, stats);

    WindData wind = wind::obtain(opts.wind_mode, opts.wind_path, track, i, ntracks);
    const WindData* wp = wind.valid ? &wind : nullptr;

    PowerAnalysis pa = reader.estimate_power(opts.power, i, wp);

    std::vector<Hill> hills = reader.detect_hills(i);
    reader.attach_climb_power(hills, pa);
    io::print_hills(hills);

    io::print_power_stats(pa.stats, opts.power, opts.body_mass_kg);

    // --- Training analysis -------------------------------------------------
    metrics::TrainingLoad tl =
        metrics::training_load(track, pa, opts.ftp_w, opts.body_mass_kg);
    io::print_training_load(tl);
    io::print_decoupling(metrics::decoupling(track, pa));
    io::print_zone_table(zones::power_zones(track, pa, opts.ftp_w));
    io::print_zone_table(zones::hr_zones(track, pa, opts.lthr, opts.max_hr));
    io::print_zone_table(zones::cadence_zones(track, pa));
    if (opts.split_km > 0.0)
        io::print_splits(splits::by_distance(track, pa, opts.split_km));

    // Critical-power model from an internally-computed mean-maximal curve.
    static const std::vector<Long> kCpDurations = {
        60, 120, 180, 300, 420, 600, 900, 1200};
    PowerCurve cp_curve = reader.power_curve(pa, kCpDurations, i);
    cp::CpFit  cp_fit   = cp::fit(cp_curve);
    io::print_cp(cp_fit);

    // Peak efforts (with timestamps) and quadrant analysis (force vs cadence).
    static const std::vector<Long> kPeakDurations = {
        5, 15, 30, 60, 300, 600, 1200, 1800, 3600};
    io::print_peaks(peaks::best_efforts(track, pa, kPeakDurations));

    // Durations long enough for fatigue to show, against work levels a long
    // ride passes through. Short rides drop the thresholds they never reach.
    static const std::vector<Long> kDurabilityDurations = {60, 300, 1200};
    static const std::vector<Real> kWorkThresholds      = {0.0, 500.0, 1000.0,
                                                           1500.0, 2000.0, 2500.0};
    io::print_durability(
        durability::analyse(track, pa, kDurabilityDurations, kWorkThresholds));
    io::print_quadrant(quadrant::analyse(track, pa, opts.ftp_w, opts.crank_length_m));

    // W'-balance series drives both the match summary and the optional export.
    std::vector<cp::WbalSample> wbal;
    if (cp_fit.valid) {
        wbal = cp::wbal_series(track, pa, cp_fit.cp_w, cp_fit.w_prime_j);
        io::print_matches(cp::count_matches(wbal, cp_fit.w_prime_j));
    }

    // --- File exports ------------------------------------------------------
    if (!opts.power_csv.empty()) {
        const std::string p = outp(opts.power_csv);
        if (io::write_power_csv(p, track, pa))
            std::cout << "Wrote time-vs-power CSV: " << p << "\n\n";
        else
            std::cerr << "Error: could not write CSV to " << p << "\n";
    }

    if (!opts.xy_path.empty()) {
        const std::string p = outp(opts.xy_path);
        if (io::write_xy_file(p, track, stats, pa))
            std::cout << "Wrote XY data table: " << p << "\n\n";
        else
            std::cerr << "Error: could not write XY table to " << p << "\n";
    }

    if (!opts.power_curve_path.empty()) {
        static const std::vector<Long> kCurveDurations = {
            1, 5, 10, 30, 60, 300, 600, 1200, 1800, 3600, 5400};
        PowerCurve curve = reader.power_curve(pa, kCurveDurations, i);
        const std::string p = outp(opts.power_curve_path);
        if (!curve.valid)
            std::cerr << "Power curve: no usable power/time data — skipping " << p << "\n";
        else if (io::write_power_curve_file(p, curve))
            std::cout << "Wrote power curve: " << p << "\n\n";
        else
            std::cerr << "Error: could not write power curve to " << p << "\n";
    }

    if (!opts.power_hist_path.empty()) {
        PowerHistogram hist = reader.power_histogram(pa, opts.hist_bin_w, i);
        const std::string p = outp(opts.power_hist_path);
        if (!hist.valid)
            std::cerr << "Power histogram: no usable power/time data — skipping " << p << "\n";
        else if (io::write_power_hist_file(p, hist))
            std::cout << "Wrote power histogram: " << p << "\n\n";
        else
            std::cerr << "Error: could not write power histogram to " << p << "\n";
    }

    if (!opts.wbal_path.empty()) {
        const std::string p = outp(opts.wbal_path);
        if (!cp_fit.valid)
            std::cerr << "W'-balance: no CP fit — skipping " << p << "\n";
        else if (io::write_wbal_file(p, wbal, cp_fit.cp_w, cp_fit.w_prime_j))
            std::cout << "Wrote W'-balance: " << p << "\n\n";
        else
            std::cerr << "Error: could not write W'-balance to " << p << "\n";
    }

    // Autocorrelation + power spectrum: one flag per quantity.
    const std::vector<std::pair<std::string, std::string>> acf_requests = {
        {"velocity",       opts.acf_velocity},
        {"power",          opts.acf_power},
        {"power_measured", opts.acf_power_measured},
        {"hr",             opts.acf_hr},
        {"cadence",        opts.acf_cadence},
        {"torque",         opts.acf_torque},
    };
    const Bool any_acf = std::any_of(
        acf_requests.begin(), acf_requests.end(),
        [](const auto& r) { return !r.second.empty(); });
    if (any_acf) {
        std::vector<channels::Channel> chans = channels::extract(track, stats, pa);
        for (const auto& [name, req] : acf_requests) {
            if (req.empty()) continue;
            const std::string p = outp(req);
            const channels::Channel* ch = nullptr;
            for (const channels::Channel& c : chans)
                if (c.name == name) { ch = &c; break; }
            if (!ch) {
                std::cerr << "ACF/PSD: '" << name
                          << "' not present in this track — skipping " << p << "\n";
                continue;
            }
            signal::SpectralResult sr = signal::compute_acf_psd(ch->t_s, ch->value, opts.acf_dt);
            if (!sr.valid)
                std::cerr << "ACF/PSD: not enough / constant data for " << name
                          << " — skipping " << p << "\n";
            else if (io::write_spectral_file(p, ch->name, ch->unit, sr))
                std::cout << "Wrote autocorrelation/spectrum: " << p
                          << " (dt=" << sr.dt_s << "s, " << sr.n_samples << " samples)\n";
            else
                std::cerr << "Error: could not write " << p << "\n";
        }
        std::cout << "\n";
    }

    // Fastest segments by distance and by time.
    for (Real d_km : opts.dist_windows) {
        std::ostringstream lbl;
        lbl << std::fixed << std::setprecision(2) << d_km << " km";
        io::print_best_segment(reader.fastest_by_distance(d_km * 1000.0, i), lbl.str());
    }
    for (Long t_s : opts.time_windows)
        io::print_best_segment(reader.fastest_by_time(t_s, i), io::format_duration(t_s));

    return tl.valid ? tl.tss : 0.0;
}

// The basename of a path, for display (e.g. "rides/a.gpx" -> "a.gpx").
std::string basename_of(const std::string& path) {
    const Size slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

} // namespace

Int run(const arg_parser::Options& opts) {
    const Bool multi = opts.filepaths.size() > 1;
    std::vector<trends::RideLoad> ride_loads;   // one per file, for the trend
    Int status = EXIT_SUCCESS;

    for (Size fi = 0; fi < opts.filepaths.size(); ++fi) {
        const std::string& filepath = opts.filepaths[fi];

        GpxReader reader;
        if (!reader.parse(filepath)) {
            std::cerr << "Error: " << filepath << ": " << reader.error_message() << "\n";
            status = EXIT_FAILURE;
            continue;
        }
        const GpxData& data = reader.data();

        std::cout << "\n========== " << filepath << " ==========\n";
        if (!data.metadata_time.empty())
            std::cout << "Recorded   : " << data.metadata_time << "\n";
        std::cout << "Tracks     : " << data.tracks.size() << "\n";

        // A date for the training trend: metadata time, else first point.
        std::string ride_date;
        if (data.metadata_time.size() >= 10) ride_date = data.metadata_time.substr(0, 10);

        const std::string file_tag = multi ? (".f" + std::to_string(fi)) : "";
        Real  file_tss     = 0.0;
        Bool  file_has_tss = false;

        for (Size i = 0; i < data.tracks.size(); ++i) {
            const Real tss = analyse_track(reader, i, opts, file_tag, data.tracks.size());
            if (tss > 0.0) { file_tss += tss; file_has_tss = true; }
            if (ride_date.empty() && !data.tracks[i].points.empty() &&
                data.tracks[i].points.front().time.size() >= 10)
                ride_date = data.tracks[i].points.front().time.substr(0, 10);
        }

        if (file_has_tss && !ride_date.empty())
            ride_loads.push_back({ride_date, basename_of(filepath), file_tss});
    }

    if (multi)
        io::print_trends(trends::progression(ride_loads));

    return status;
}

} // namespace app
