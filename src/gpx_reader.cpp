#include "gpx_reader.h"

#include <pugixml.hpp>
#include <cmath>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

/// Haversine distance between two WGS-84 points, result in metres.
double haversine(double lat1, double lon1, double lat2, double lon2) {
    constexpr double R = 6'371'000.0; // Earth radius in metres
    constexpr double DEG2RAD = M_PI / 180.0;

    const double phi1 = lat1 * DEG2RAD;
    const double phi2 = lat2 * DEG2RAD;
    const double dphi = (lat2 - lat1) * DEG2RAD;
    const double dlam = (lon2 - lon1) * DEG2RAD;

    const double a = std::sin(dphi / 2) * std::sin(dphi / 2)
                   + std::cos(phi1) * std::cos(phi2)
                   * std::sin(dlam / 2) * std::sin(dlam / 2);

    return R * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

/// Parse an ISO-8601 timestamp string ("2026-03-31T09:46:45.000Z") to time_t.
/// Returns -1 on failure.
std::time_t parse_iso8601(const std::string& s) {
    std::tm tm{};
    // strptime handles "2026-03-31T09:46:45" — ignore fractional seconds and Z
    const char* p = strptime(s.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
    if (!p) return -1;
    tm.tm_isdst = 0;
    // timegm interprets as UTC (available on Linux/glibc)
    return timegm(&tm);
}

} // namespace

// ---------------------------------------------------------------------------
// GpxReader::parse
// ---------------------------------------------------------------------------

bool GpxReader::parse(const std::string& filepath) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filepath.c_str());

    if (!result) {
        error_ = std::string("XML parse error: ") + result.description()
               + " (offset " + std::to_string(result.offset) + ")";
        return false;
    }

    // Root element: <gpx>
    pugi::xml_node gpx = doc.child("gpx");
    if (!gpx) {
        error_ = "No <gpx> root element found.";
        return false;
    }

    // <metadata><time>
    pugi::xml_node metadata = gpx.child("metadata");
    if (metadata) {
        data_.metadata_time = metadata.child("time").child_value();
    }

    // Iterate over all <trk> elements
    for (pugi::xml_node trk : gpx.children("trk")) {
        Track track;
        track.name = trk.child("name").child_value();
        track.type = trk.child("type").child_value();

        // Iterate over all <trkseg> segments
        for (pugi::xml_node seg : trk.children("trkseg")) {
            for (pugi::xml_node trkpt : seg.children("trkpt")) {
                TrackPoint pt;
                pt.lat = trkpt.attribute("lat").as_double();
                pt.lon = trkpt.attribute("lon").as_double();
                pt.ele = trkpt.child("ele").text().as_double();
                pt.time = trkpt.child("time").child_value();

                // Extensions: look for ns3:atemp
                pugi::xml_node ext = trkpt.child("extensions");
                if (ext) {
                    // The namespace prefix is part of the element name in pugixml
                    pugi::xml_node tpe = ext.child("ns3:TrackPointExtension");
                    if (tpe) {
                        pugi::xml_node atemp_node = tpe.child("ns3:atemp");
                        if (atemp_node) {
                            pt.atemp = atemp_node.text().as_double();
                            pt.has_atemp = true;
                        }
                    }
                }

                track.points.push_back(std::move(pt));
            }
        }

        data_.tracks.push_back(std::move(track));
    }

    return true;
}

// ---------------------------------------------------------------------------
// GpxReader::compute_stats
// ---------------------------------------------------------------------------

TrackStats GpxReader::compute_stats(std::size_t track_index) const {
    TrackStats stats;

    if (track_index >= data_.tracks.size()) return stats;

    const Track& track = data_.tracks[track_index];
    const auto&  pts   = track.points;

    stats.num_points = pts.size();
    if (pts.empty()) return stats;

    stats.min_ele_m = pts.front().ele;
    stats.max_ele_m = pts.front().ele;

    double atemp_sum   = 0.0;
    std::size_t atemp_count = 0;

    double climb_grade_sum  = 0.0;
    double descent_grade_sum = 0.0;
    std::size_t climb_count  = 0;
    std::size_t descent_count = 0;

    for (std::size_t i = 0; i < pts.size(); ++i) {
        const auto& p = pts[i];

        // Elevation extremes
        if (p.ele < stats.min_ele_m) stats.min_ele_m = p.ele;
        if (p.ele > stats.max_ele_m) stats.max_ele_m = p.ele;

        // Temperature
        if (p.has_atemp) {
            atemp_sum += p.atemp;
            ++atemp_count;
        }

        if (i > 0) {
            const auto& prev = pts[i - 1];

            // Distance (Haversine, horizontal)
            const double horiz_dist = haversine(prev.lat, prev.lon, p.lat, p.lon);
            stats.total_distance_m += horiz_dist;

            // Elevation gain/loss
            const double delta = p.ele - prev.ele;
            if (delta > 0.0) stats.elevation_gain_m += delta;
            else             stats.elevation_loss_m += -delta;

            // Gradient — only for steps with meaningful horizontal distance
            if (horiz_dist >= 1.0) {
                const double grade = (delta / horiz_dist) * 100.0;
                if (delta > 0.0) {
                    climb_grade_sum += grade;
                    ++climb_count;
                } else if (delta < 0.0) {
                    descent_grade_sum += grade;
                    ++descent_count;
                }
            }
        }
    }

    // Duration
    std::time_t t_start = parse_iso8601(pts.front().time);
    std::time_t t_end   = parse_iso8601(pts.back().time);
    if (t_start != -1 && t_end != -1 && t_end >= t_start) {
        stats.duration_s = static_cast<long>(t_end - t_start);
    }

    // Average speed (km/h)
    if (stats.duration_s > 0) {
        stats.avg_speed_kmh = (stats.total_distance_m / 1000.0)
                            / (stats.duration_s / 3600.0);
    }

    // Average temperature
    if (atemp_count > 0) {
        stats.avg_atemp = atemp_sum / static_cast<double>(atemp_count);
        stats.has_atemp = true;
    }

    // Average climb / descent gradient
    if (climb_count > 0)
        stats.avg_climb_pct = climb_grade_sum / static_cast<double>(climb_count);
    if (descent_count > 0)
        stats.avg_descent_pct = descent_grade_sum / static_cast<double>(descent_count);

    return stats;
}

// ---------------------------------------------------------------------------
// Internal helper: build a cumulative distance table and timestamp table
// ---------------------------------------------------------------------------

namespace {

/// Precompute cumulative Haversine distances and parsed timestamps for a track.
/// cum_dist[i] = total distance from point 0 to point i, in metres.
/// timestamps[i] = time_t for point i (-1 if unparseable).
void build_tables(const std::vector<TrackPoint>& pts,
                  std::vector<double>&           cum_dist,
                  std::vector<std::time_t>&      timestamps)
{
    const std::size_t n = pts.size();
    cum_dist.resize(n, 0.0);
    timestamps.resize(n, -1);

    for (std::size_t i = 0; i < n; ++i) {
        timestamps[i] = parse_iso8601(pts[i].time);
        if (i > 0) {
            cum_dist[i] = cum_dist[i - 1]
                        + haversine(pts[i-1].lat, pts[i-1].lon,
                                    pts[i].lat,   pts[i].lon);
        }
    }
}

/// Fill a BestSegment from two indices and the precomputed tables.
BestSegment make_segment(std::size_t                    lo,
                         std::size_t                    hi,
                         const std::vector<TrackPoint>& pts,
                         const std::vector<double>&     cum_dist,
                         const std::vector<std::time_t>& timestamps)
{
    BestSegment seg;
    seg.valid       = true;
    seg.start_idx   = lo;
    seg.end_idx     = hi;
    seg.distance_m  = cum_dist[hi] - cum_dist[lo];
    seg.duration_s  = static_cast<long>(timestamps[hi] - timestamps[lo]);
    seg.start_time  = pts[lo].time;
    seg.end_time    = pts[hi].time;
    seg.start_lat   = pts[lo].lat;
    seg.start_lon   = pts[lo].lon;
    seg.end_lat     = pts[hi].lat;
    seg.end_lon     = pts[hi].lon;
    if (seg.duration_s > 0) {
        seg.avg_speed_kmh = (seg.distance_m / 1000.0)
                          / (seg.duration_s / 3600.0);
    }
    return seg;
}

} // namespace

// ---------------------------------------------------------------------------
// GpxReader::fastest_by_distance
// ---------------------------------------------------------------------------

BestSegment GpxReader::fastest_by_distance(double      window_m,
                                            std::size_t track_index) const
{
    BestSegment best;
    if (track_index >= data_.tracks.size()) return best;

    const auto& pts = data_.tracks[track_index].points;
    if (pts.size() < 2) return best;

    std::vector<double>      cum_dist;
    std::vector<std::time_t> timestamps;
    build_tables(pts, cum_dist, timestamps);

    // Check that the whole track is at least as long as the requested window
    if (cum_dist.back() < window_m) return best;  // valid stays false

    std::size_t hi = 1;
    for (std::size_t lo = 0; lo < pts.size() - 1; ++lo) {
        // Advance hi until the window covers at least window_m metres
        while (hi < pts.size() - 1 &&
               (cum_dist[hi] - cum_dist[lo]) < window_m) {
            ++hi;
        }

        // Skip if timestamps are invalid
        if (timestamps[lo] < 0 || timestamps[hi] < 0) continue;
        const long dur = static_cast<long>(timestamps[hi] - timestamps[lo]);
        if (dur <= 0) continue;

        const double dist = cum_dist[hi] - cum_dist[lo];
        if (dist < window_m) continue;  // couldn't fill the window

        // Speed over the exact window distance (using actual elapsed time)
        const double speed = (dist / 1000.0) / (dur / 3600.0);

        if (!best.valid || speed > best.avg_speed_kmh) {
            best = make_segment(lo, hi, pts, cum_dist, timestamps);
        }
    }

    return best;
}

// ---------------------------------------------------------------------------
// GpxReader::fastest_by_time
// ---------------------------------------------------------------------------

BestSegment GpxReader::fastest_by_time(long        window_s,
                                        std::size_t track_index) const
{
    BestSegment best;
    if (track_index >= data_.tracks.size()) return best;

    const auto& pts = data_.tracks[track_index].points;
    if (pts.size() < 2) return best;

    std::vector<double>      cum_dist;
    std::vector<std::time_t> timestamps;
    build_tables(pts, cum_dist, timestamps);

    // Check that the whole track is at least as long as the requested window
    const long total_dur = static_cast<long>(
        timestamps.back() - timestamps.front());
    if (total_dur < window_s) return best;  // valid stays false

    std::size_t hi = 1;
    for (std::size_t lo = 0; lo < pts.size() - 1; ++lo) {
        if (timestamps[lo] < 0) continue;

        // Advance hi until the window spans at least window_s seconds
        while (hi < pts.size() - 1 &&
               timestamps[hi] >= 0 &&
               (timestamps[hi] - timestamps[lo]) < window_s) {
            ++hi;
        }

        if (timestamps[hi] < 0) continue;
        const long dur = static_cast<long>(timestamps[hi] - timestamps[lo]);
        if (dur < window_s) continue;

        const double dist = cum_dist[hi] - cum_dist[lo];
        const double speed = (dist / 1000.0) / (dur / 3600.0);

        if (!best.valid || speed > best.avg_speed_kmh) {
            best = make_segment(lo, hi, pts, cum_dist, timestamps);
        }
    }

    return best;
}

// ---------------------------------------------------------------------------
// GpxReader::detect_hills
// ---------------------------------------------------------------------------

std::vector<Hill> GpxReader::detect_hills(std::size_t track_index) const
{
    std::vector<Hill> hills;
    if (track_index >= data_.tracks.size()) return hills;

    const auto& pts = data_.tracks[track_index].points;
    if (pts.size() < 2) return hills;

    // Detection thresholds
    constexpr double MIN_GRADE_PCT    =  1.0;  // step must be at least this steep
    constexpr double MIN_GAIN_M       = 10.0;  // completed climb must gain this much
    constexpr double GAP_TOLERANCE_M  = 20.0;  // flat/descent absorbed before hill ends

    // Precompute cumulative distances
    const std::size_t n = pts.size();
    std::vector<double> cum_dist(n, 0.0);
    for (std::size_t i = 1; i < n; ++i)
        cum_dist[i] = cum_dist[i-1] + haversine(pts[i-1].lat, pts[i-1].lon,
                                                 pts[i].lat,   pts[i].lon);

    // State machine
    bool        climbing   = false;
    std::size_t hill_start = 0;   // index where current hill began
    std::size_t peak_idx   = 0;   // index of highest point so far in current hill
    double      peak_ele   = 0.0; // elevation at peak_idx
    double      gap_loss   = 0.0; // cumulative descent since last peak

    auto commit_hill = [&](std::size_t end_idx) {
        const double gain = pts[end_idx].ele - pts[hill_start].ele;
        if (gain < MIN_GAIN_M) return;   // too small — discard

        const double dist = cum_dist[end_idx] - cum_dist[hill_start];
        if (dist < 1.0) return;          // degenerate

        Hill h;
        h.start_idx     = hill_start;
        h.end_idx       = end_idx;
        h.distance_m    = dist;
        h.gain_m        = gain;
        h.start_ele_m   = pts[hill_start].ele;
        h.end_ele_m     = pts[end_idx].ele;
        h.avg_grade_pct = (gain / dist) * 100.0;
        h.start_time    = pts[hill_start].time;
        h.end_time      = pts[end_idx].time;
        hills.push_back(h);
    };

    for (std::size_t i = 1; i < n; ++i) {
        const double horiz = cum_dist[i] - cum_dist[i-1];
        const double delta = pts[i].ele - pts[i-1].ele;
        const double grade = (horiz >= 1.0) ? (delta / horiz) * 100.0 : 0.0;

        if (!climbing) {
            // Enter climbing state when we see a steep enough uphill step
            if (grade >= MIN_GRADE_PCT) {
                climbing   = true;
                hill_start = i - 1;
                peak_idx   = i;
                peak_ele   = pts[i].ele;
                gap_loss   = 0.0;
            }
        } else {
            // Already climbing
            if (pts[i].ele > peak_ele) {
                // New high point — reset gap tracker
                peak_ele = pts[i].ele;
                peak_idx = i;
                gap_loss = 0.0;
            } else if (delta < 0.0) {
                // Accumulate descent into gap loss
                gap_loss += -delta;
            }

            // End the hill if gap tolerance is exceeded
            if (gap_loss > GAP_TOLERANCE_M) {
                commit_hill(peak_idx);
                climbing = false;
                gap_loss = 0.0;

                // The current step might already be the start of a new hill
                if (grade >= MIN_GRADE_PCT) {
                    climbing   = true;
                    hill_start = i - 1;
                    peak_idx   = i;
                    peak_ele   = pts[i].ele;
                    gap_loss   = 0.0;
                }
            }
        }
    }

    // Commit any hill still open at the end of the track
    if (climbing)
        commit_hill(peak_idx);

    return hills;
}
