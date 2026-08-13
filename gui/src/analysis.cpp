#include "analysis.hpp"

#include "app.hpp"          // app::run
#include "arg_parser.hpp"   // arg_parser::Options
#include "channels.hpp"     // channels::extract
#include "gpx_reader.hpp"   // GpxReader, Track, PowerAnalysis
#include "peaks.hpp"        // peaks::best_efforts
#include "wind.hpp"         // wind::obtain
#include "zones.hpp"        // zones::power_zones

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <limits>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

namespace gui {

namespace {

// Redirects std::cout and std::cerr into string buffers for the duration of a
// scope, restoring them in the destructor so an exception on the way out cannot
// leave the streams pointing at dead buffers.
class StreamCapture {
public:
    StreamCapture()
        : old_out_(std::cout.rdbuf(out_.rdbuf())),
          old_err_(std::cerr.rdbuf(err_.rdbuf())) {}

    ~StreamCapture() {
        std::cout.rdbuf(old_out_);
        std::cerr.rdbuf(old_err_);
    }

    StreamCapture(const StreamCapture&)            = delete;
    StreamCapture& operator=(const StreamCapture&) = delete;

    std::string out() const { return out_.str(); }
    std::string err() const { return err_.str(); }

private:
    std::ostringstream out_;
    std::ostringstream err_;
    std::streambuf*    old_out_;
    std::streambuf*    old_err_;
};

// Elevation as a channel, alongside the ones channels::extract() builds.
// Elevation and temperature as channels, alongside the ones
// channels::extract() builds.
//
// They live here rather than in the library because the analysis has no use for
// either -- the hills table works from the raw points and the temperature only
// reaches the statistics block -- while a plot of the ride wants both on the
// same time axis as everything else. Same shape as the library's channels, so
// they plot and transform like any other.

/// Build a channel from a per-point field.
///
/// @param track     The track to read.
/// @param pa        Supplies the per-point elapsed times.
/// @param name      Channel name.
/// @param unit      Channel unit.
/// @param present   Whether a given point carries the field.
/// @param value     The field's value at a given point.
template <typename Present, typename Value>
channels::Channel point_channel(const Track& track, const PowerAnalysis& pa,
                                const char* name, const char* unit,
                                Present present, Value value) {
    channels::Channel c{name, unit, {}, {}};

    for (Size i = 0; i < track.points.size(); ++i) {
        const Long t = (i < pa.t_offset_s.size()) ? pa.t_offset_s[i] : -1;
        if (t < 0 || !present(track.points[i])) continue;
        c.t_s.push_back(static_cast<Real>(t));
        c.value.push_back(value(track.points[i]));
    }
    return c;
}

/// Whether a series varies enough to be worth an axis of its own.
///
/// TrackPoint has no has_ele flag -- the parser leaves elevation at 0.0 when the
/// file carries no <ele> -- so a track without elevation is indistinguishable
/// from one recorded at sea level, and would plot as a flat line. Requiring some
/// variation stands in for the missing flag.
bool varies_by(const channels::Channel& c, Real minimum) {
    if (c.value.size() < 2) return false;
    const auto [lo, hi] = std::minmax_element(c.value.begin(), c.value.end());
    return (*hi - *lo) >= minimum;
}

/// The cumulative-distance axis, from the series the power analysis already
/// accumulates -- the same one the --xy export writes as distance_km.
DistanceAxis distance_axis(const PowerAnalysis& pa) {
    DistanceAxis d;
    const Size n = std::min(pa.t_offset_s.size(), pa.cum_dist_m.size());
    for (Size i = 0; i < n; ++i) {
        if (pa.t_offset_s[i] < 0) continue;
        d.t_s.push_back(static_cast<Real>(pa.t_offset_s[i]));
        d.km.push_back(pa.cum_dist_m[i] / 1000.0);
    }
    return d;
}

/// Append the channels the library does not build, when the track carries them.
void add_point_channels(const Track& track, const PowerAnalysis& pa,
                        std::vector<channels::Channel>& out) {
    channels::Channel ele = point_channel(
        track, pa, "elevation", "m",
        [](const TrackPoint&) { return true; },
        [](const TrackPoint& p) { return p.ele; });
    if (varies_by(ele, 1.0)) out.push_back(std::move(ele));

    // "C" rather than "\u00b0C": ImGui's default font is ASCII only, so the
    // degree sign would render as a placeholder box.
    channels::Channel temp = point_channel(
        track, pa, "temperature", "C",
        [](const TrackPoint& p) { return p.has_atemp; },
        [](const TrackPoint& p) { return p.atemp; });
    if (temp.t_s.size() >= 2) out.push_back(std::move(temp));
}

// Which power zone `watts` falls in, or -1 when the zone table is unusable.
int zone_of(Real watts, const zones::ZoneTable& table) {
    if (!table.valid) return -1;
    for (Size k = 0; k < table.zones.size(); ++k) {
        const zones::Zone& z = table.zones[k];
        const bool         open_top = z.hi < 0.0;
        if (watts >= z.lo && (open_top || watts < z.hi)) return static_cast<int>(k);
    }
    return -1;
}

// Two neighbouring stretches count as "the same steepness" while their
// gradients are within this many percentage points.
constexpr Real kGradeTolPct = 1.5;

// The remaining thresholds scale with the climb, because one set of absolute
// lengths cannot serve both a 1 km ramp and an 8 km pass. Fixed at the values
// that suit a long climb, a short one loses its shape -- a 400 m floor on a
// 1.3 km climb swallows a 350 m descent in the middle and reports the rise and
// the dip together as one 0 % section.

// Shortest stretch the gradient is measured over: point-to-point gradients from
// GPS elevation are far too noisy to segment on directly.
Real block_len_m(Real total_m) {
    return std::clamp(total_m / 25.0, 50.0, 200.0);
}

// Shortest stretch that survives. Below it, a stretch is folded into whichever
// neighbour it resembles more; without this a gradient that wanders across the
// tolerance leaves a row of unreadable slivers.
Real min_seg_m(Real total_m) {
    return std::clamp(total_m * 0.08, 100.0, 400.0);
}

// How many sections stay readable on one plot.
Size max_seg_count(Real total_m) {
    return std::clamp<Size>(static_cast<Size>(total_m / 300.0 + 0.5), 3, 10);
}

// Split a climb into stretches of roughly constant gradient, then average the
// power over each.
//
// The climb is chopped into short blocks, then neighbours are merged
// agglomeratively: repeatedly join the adjacent pair whose gradients are
// closest. Merging by closest pair rather than left to right matters -- a single
// left-to-right pass compares each block against the running mean, so an
// oscillating gradient shatters the climb into slivers.
//
// Merging continues while any adjacent pair is within kGradeTolPct, then while
// any stretch is below the length floor, and finally until the section count is
// readable. Because the closest pair goes first, a rise and a descent are never
// merged while any similar pair remains: their gradients differ by twice the
// gradient, far above the tolerance.
std::vector<HillSegment> segment_by_gradient(const HillProfile& p,
                                             const zones::ZoneTable& zt) {
    std::vector<HillSegment> out;
    const Size n = p.ele_m.size();
    if (n < 2) return out;

    auto dist_m = [&](Size b, Size e) {
        return (p.dist_km[e] - p.dist_km[b]) * 1000.0;
    };
    auto grade = [&](Size b, Size e) {
        const Real d = dist_m(b, e);
        return d > 1.0 ? (p.ele_m[e] - p.ele_m[b]) / d * 100.0 : 0.0;
    };

    // --- thresholds, scaled to this climb's length ---------------------------
    const Real total_m  = dist_m(0, n - 1);
    const Real block_m  = block_len_m(total_m);
    const Real floor_m  = min_seg_m(total_m);
    const Size max_segs = max_seg_count(total_m);

    // --- blocks of at least block_m -----------------------------------------
    std::vector<std::pair<Size, Size>> seg;
    for (Size b = 0; b + 1 < n;) {
        Size e = b + 1;
        while (e + 1 < n && dist_m(b, e) < block_m) ++e;
        seg.emplace_back(b, e);
        b = e;
    }
    if (seg.empty()) return out;

    auto merge_at = [&](Size i) {
        seg[i].second = seg[i + 1].second;
        seg.erase(seg.begin() + static_cast<long>(i) + 1);
    };

    // Index of the adjacent pair with the most similar gradients.
    auto closest_pair = [&]() {
        Size best = 0;
        Real best_d = std::numeric_limits<Real>::max();
        for (Size i = 0; i + 1 < seg.size(); ++i) {
            const Real d = std::abs(grade(seg[i].first, seg[i].second) -
                                    grade(seg[i + 1].first, seg[i + 1].second));
            if (d < best_d) { best_d = d; best = i; }
        }
        return std::pair<Size, Real>{best, best_d};
    };

    // 1. join everything that counts as the same steepness
    while (seg.size() > 1) {
        const auto [i, d] = closest_pair();
        if (d > kGradeTolPct) break;
        merge_at(i);
    }

    // 2. absorb stretches too short to read, into the closer-gradient neighbour
    for (bool again = true; again && seg.size() > 1;) {
        again = false;
        for (Size i = 0; i < seg.size(); ++i) {
            if (dist_m(seg[i].first, seg[i].second) >= floor_m) continue;
            Size j = i;                             // pair (j, j+1) to merge
            if (i == 0)                     j = 0;
            else if (i + 1 == seg.size())   j = i - 1;
            else {
                const Real g = grade(seg[i].first, seg[i].second);
                j = std::abs(grade(seg[i - 1].first, seg[i - 1].second) - g) <=
                    std::abs(grade(seg[i + 1].first, seg[i + 1].second) - g)
                        ? i - 1 : i;
            }
            merge_at(j);
            again = true;
            break;
        }
    }

    // 3. keep the count readable
    while (seg.size() > max_segs) merge_at(closest_pair().first);

    // --- finalise: gradient and mean power over each stretch ----------------
    out.reserve(seg.size());
    for (const auto& [b, e] : seg) {
        HillSegment s;
        s.begin         = b;
        s.end           = e;
        s.dist_m        = dist_m(b, e);
        s.avg_grade_pct = grade(b, e);

        // Each entry of power_w is the power on the step arriving at that
        // sample, so this stretch's own steps are (begin, end].
        Real sum = 0.0;
        Size cnt = 0;
        for (Size k = b + 1; k <= e && k < p.power_w.size(); ++k) {
            sum += p.power_w[k];
            ++cnt;
        }
        if (cnt > 0) {
            s.avg_power_w = sum / static_cast<Real>(cnt);
            s.has_power   = true;
            s.zone        = zone_of(s.avg_power_w, zt);
        }
        out.push_back(s);
    }
    return out;
}

// Build one climb's profile: elevation against distance and against time, both
// measured from the foot of the climb.
HillProfile hill_profile(const Hill& h, const Track& track, const PowerAnalysis& pa) {
    HillProfile p;
    p.distance_km   = h.distance_m / 1000.0;
    p.gain_m        = h.gain_m;
    p.avg_grade_pct = h.avg_grade_pct;
    p.vam_mh        = h.vam_mh;
    p.avg_power_w   = h.avg_power_w;
    p.has_power     = h.has_power;
    p.duration_s    = h.duration_s;
    p.category      = h.category;
    p.start_time    = h.start_time;

    const std::vector<TrackPoint>& pts = track.points;
    if (pa.cum_dist_m.size() != pts.size() || pa.t_offset_s.size() != pts.size())
        return p;

    const Size last = std::min(h.end_idx, pts.size() - 1);
    if (h.start_idx > last) return p;

    const Real d0 = pa.cum_dist_m[h.start_idx];
    const Long t0 = pa.t_offset_s[h.start_idx];
    bool       timed = t0 >= 0;

    for (Size i = h.start_idx; i <= last; ++i) {
        p.dist_km.push_back((pa.cum_dist_m[i] - d0) / 1000.0);
        p.ele_m.push_back(pts[i].ele);
        p.power_w.push_back(i < pa.point_power_w.size() ? pa.point_power_w[i] : 0.0);
        if (timed && pa.t_offset_s[i] >= 0)
            p.time_min.push_back(static_cast<Real>(pa.t_offset_s[i] - t0) / 60.0);
        else
            timed = false;
    }
    // A gap in the timestamps leaves the time axis short of the curve, so it is
    // dropped entirely rather than drawn misaligned.
    if (!timed || p.time_min.size() != p.ele_m.size()) p.time_min.clear();
    return p;
}

// Re-run just the steps the charts need, to get the numbers as structs rather
// than as printed text. app::run() prints its report and keeps nothing, so the
// parse and power estimate happen a second time here -- about 0.1 s on a large
// ride, which is not worth reimplementing app::run() to avoid.
void collect_chart_data(const arg_parser::Options& opts, Result& result) {
    GpxReader reader;
    if (!reader.parse(opts.filepaths.front())) return;

    const std::vector<Track>& tracks = reader.data().tracks;
    result.tracks.resize(tracks.size());

    for (Size i = 0; i < tracks.size(); ++i) {
        // The same acquisition app::run() performs before its own
        // estimate_power, so the charts and the text report describe one ride.
        // With wind_mode == Off this is a no-op and nothing is fetched.
        const WindData  w  = wind::obtain(opts.wind_mode, opts.wind_path,
                                          tracks[i], i, tracks.size());
        const WindData* wp = w.valid ? &w : nullptr;

        const PowerAnalysis pa = reader.estimate_power(opts.power, i, wp);
        TrackCharts&        tc = result.tracks[i];

        tc.power_zones = zones::power_zones(tracks[i], pa, opts.ftp_w);
        tc.ftp_w       = opts.ftp_w;
        tc.channels    = channels::extract(tracks[i], reader.compute_stats(i), pa);

        add_point_channels(tracks[i], pa, tc.channels);
        tc.distance = distance_axis(pa);

        // Same durations the report uses. They live in a local constant inside
        // app.cpp, so the list is repeated here rather than shared; if the two
        // drift, the chart and the table beside it stop matching.
        static const std::vector<Long> kPeakDurations = {
            5, 15, 30, 60, 300, 600, 1200, 1800, 3600};

        for (const peaks::PeakEffort& e :
             peaks::best_efforts(tracks[i], pa, kPeakDurations)) {
            PeakBar b;
            b.duration_s     = e.duration_s;
            b.avg_power_w    = e.avg_power_w;
            b.wkg            = opts.body_mass_kg > 0.0
                                   ? e.avg_power_w / opts.body_mass_kg : 0.0;
            b.start_offset_s = e.start_offset_s;
            b.measured       = e.measured;
            b.zone           = zone_of(e.avg_power_w, tc.power_zones);
            tc.peaks.push_back(b);
        }

        std::vector<Hill> hills = reader.detect_hills(i);
        reader.attach_climb_power(hills, pa);
        tc.hills.reserve(hills.size());
        for (const Hill& h : hills) {
            HillProfile p = hill_profile(h, tracks[i], pa);
            p.segments    = segment_by_gradient(p, tc.power_zones);
            tc.hills.push_back(std::move(p));
        }
    }
}

} // namespace

Result analyse(const std::string& gpx_path, std::size_t max_print,
               bool use_wind) {
    Result result;

    // Build the options the same way the command line does, by handing a
    // synthesised argv to the real parser. Filling an Options in by hand would
    // miss the defaults parse() resolves itself -- the physics mass, for one,
    // is rider + bike (80.3 kg), not PowerParams' own 80.0 kg default -- and
    // would silently drift from the CLI as more defaults are added.
    std::vector<std::string> args = {
        "gpxana_gui", gpx_path, "--points", std::to_string(max_print)};

    // Goes in as the flag the command line parses, so wind_mode is resolved by
    // the same code and the report matches a `--wind` run exactly.
    if (use_wind) args.push_back("--wind");

    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const std::string& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));

    arg_parser::Options opts;
    std::string         parse_error;
    if (!arg_parser::parse(static_cast<int>(argv.size()), argv.data(), opts,
                           parse_error)) {
        result.errors = parse_error.empty() ? "Could not build analysis options.\n"
                                            : parse_error + "\n";
        return result;
    }

    // Every export path is left empty and a single input file means no
    // multi-ride trend, so run() only prints its report. The wind flag above is
    // the one thing that reaches the network, and only when asked for.

    int         status = EXIT_FAILURE;
    std::string thrown;

    {
        StreamCapture capture;
        try {
            status = app::run(opts);
        } catch (const std::exception& e) {
            thrown = std::string("Exception during analysis: ") + e.what() + "\n";
        } catch (...) {
            thrown = "Unknown exception during analysis.\n";
        }
        result.summary = capture.out();
        result.errors  = capture.err();
    }

    result.errors += thrown;
    result.ok = thrown.empty() && status == EXIT_SUCCESS;
    if (result.ok) collect_chart_data(opts, result);
    return result;
}

} // namespace gui
