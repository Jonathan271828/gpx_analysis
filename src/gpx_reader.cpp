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

                // Extensions: look for the Garmin TrackPointExtension fields
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
                        pugi::xml_node hr_node = tpe.child("ns3:hr");
                        if (hr_node) {
                            pt.hr = hr_node.text().as_int();
                            pt.has_hr = true;
                        }
                        pugi::xml_node cad_node = tpe.child("ns3:cad");
                        if (cad_node) {
                            pt.cad = cad_node.text().as_int();
                            pt.has_cad = true;
                        }
                    }

                    // Power lives directly under <extensions> as <power> in the
                    // Garmin schema; some exporters emit <ns3:power> inside the
                    // TrackPointExtension instead — accept both.
                    pugi::xml_node pw = ext.child("power");
                    if (!pw && tpe) pw = tpe.child("ns3:power");
                    if (pw) {
                        pt.power = pw.text().as_int();
                        pt.has_power = true;
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

    long        hr_sum    = 0;  int hr_min    = 0;  int hr_max    = 0;
    std::size_t hr_count  = 0;
    long        cad_sum   = 0;  int cad_min   = 0;  int cad_max   = 0;
    std::size_t cad_count = 0;
    long        power_sum = 0;  int power_min = 0;  int power_max = 0;
    std::size_t power_count = 0;

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

        // Sensors: accumulate sum and running min/max (seed on first sample)
        if (p.has_hr) {
            if (hr_count == 0) { hr_min = hr_max = p.hr; }
            else { if (p.hr < hr_min) hr_min = p.hr; if (p.hr > hr_max) hr_max = p.hr; }
            hr_sum += p.hr;
            ++hr_count;
        }
        if (p.has_cad) {
            if (cad_count == 0) { cad_min = cad_max = p.cad; }
            else { if (p.cad < cad_min) cad_min = p.cad; if (p.cad > cad_max) cad_max = p.cad; }
            cad_sum += p.cad;
            ++cad_count;
        }
        if (p.has_power) {
            if (power_count == 0) { power_min = power_max = p.power; }
            else { if (p.power < power_min) power_min = p.power; if (p.power > power_max) power_max = p.power; }
            power_sum += p.power;
            ++power_count;
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

    // Sensor averages / extremes
    if (hr_count > 0) {
        stats.avg_hr = static_cast<double>(hr_sum) / static_cast<double>(hr_count);
        stats.min_hr = hr_min;
        stats.max_hr = hr_max;
        stats.has_hr = true;
    }
    if (cad_count > 0) {
        stats.avg_cad = static_cast<double>(cad_sum) / static_cast<double>(cad_count);
        stats.min_cad = cad_min;
        stats.max_cad = cad_max;
        stats.has_cad = true;
    }
    if (power_count > 0) {
        stats.avg_power = static_cast<double>(power_sum) / static_cast<double>(power_count);
        stats.min_power = power_min;
        stats.max_power = power_max;
        stats.has_power = true;
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

// ---------------------------------------------------------------------------
// Power model helpers
// ---------------------------------------------------------------------------

namespace {

/// Air density (kg/m^3) from elevation and (optional) air temperature.
/// Barometric pressure via the International Standard Atmosphere, then the
/// ideal-gas law with the actual temperature. Falls back to `fallback` for
/// absurd inputs. Returns ~1.225 at sea level / 15 C.
double air_density(double ele_m, double temp_c, bool has_temp, double fallback) {
    constexpr double P0 = 101325.0;        // Pa, sea-level pressure
    constexpr double T0 = 288.15;          // K,  sea-level standard temperature
    constexpr double L  = 0.0065;          // K/m, tropospheric lapse rate
    constexpr double R_SPECIFIC = 287.05;  // J/(kg K), dry air

    const double base = 1.0 - (L * ele_m) / T0;
    if (base <= 0.0) return fallback;      // guard against nonsensical elevation
    const double p = P0 * std::pow(base, 5.257);
    const double T = (has_temp ? temp_c : 15.0) + 273.15;
    if (T <= 0.0) return fallback;
    return p / (R_SPECIFIC * T);
}

/// Instantaneous pedal power (W) for one step. Sum of the four resistive
/// forces times ground speed, divided by drivetrain efficiency. `v_hw` is the
/// headwind component (0 until wind support is added). No clamping here.
double power_at_step(double v, double a, double grade, double rho,
                     const PowerParams& p, double v_hw = 0.0) {
    constexpr double G = 9.8067;           // m/s^2
    const double theta = std::atan(grade);
    const double m = p.total_mass_kg;

    const double f_gravity = m * G * std::sin(theta);
    const double f_rolling = m * G * std::cos(theta) * p.crr;
    const double v_as      = v + v_hw;     // airspeed
    // With v_hw == 0 the square is exact; for the future wind phase use
    // std::fabs(v_as) * v_as so a strong tailwind reverses the force.
    const double f_aero    = 0.5 * rho * p.cda * v_as * v_as;
    const double f_accel   = m * a;

    const double force = f_gravity + f_rolling + f_aero + f_accel;
    return (force * v) / p.drivetrain_eff;
}

} // namespace

// ---------------------------------------------------------------------------
// GpxReader::estimate_power
// ---------------------------------------------------------------------------

PowerAnalysis GpxReader::estimate_power(const PowerParams& params,
                                        std::size_t track_index) const
{
    PowerAnalysis pa;
    if (track_index >= data_.tracks.size()) return pa;

    const auto& pts = data_.tracks[track_index].points;
    const std::size_t n = pts.size();
    if (n < 2 || params.total_mass_kg <= 0.0) return pa;

    std::vector<double>      cum_dist;
    std::vector<std::time_t> timestamps;
    build_tables(pts, cum_dist, timestamps);

    pa.point_power_w.assign(n, 0.0);

    // Elapsed seconds from the first point (for the time-vs-power CSV)
    pa.t_offset_s.assign(n, -1);
    const std::time_t t0 = timestamps.front();
    if (t0 >= 0) {
        for (std::size_t i = 0; i < n; ++i)
            if (timestamps[i] >= 0)
                pa.t_offset_s[i] = static_cast<long>(timestamps[i] - t0);
    }

    double      sum = 0.0, energy_j = 0.0, max_p = 0.0;
    std::size_t count = 0;
    double      prev_v = 0.0;
    bool        have_prev_v = false;

    double      meas_sum = 0.0, abs_err_sum = 0.0, bias_sum = 0.0;
    std::size_t meas_count = 0;

    for (std::size_t i = 1; i < n; ++i) {
        // A gap in timestamps breaks the acceleration chain
        if (timestamps[i] < 0 || timestamps[i - 1] < 0) { have_prev_v = false; continue; }
        const long dt = static_cast<long>(timestamps[i] - timestamps[i - 1]);
        if (dt <= 0) { have_prev_v = false; continue; }

        const double dist  = cum_dist[i] - cum_dist[i - 1];
        const double v     = dist / static_cast<double>(dt);
        const double dele  = pts[i].ele - pts[i - 1].ele;
        const double grade = (dist >= 1.0) ? (dele / dist) : 0.0;
        const double a     = have_prev_v ? (v - prev_v) / static_cast<double>(dt) : 0.0;
        prev_v = v; have_prev_v = true;

        const double rho = air_density(pts[i].ele, pts[i].atemp, pts[i].has_atemp,
                                       params.default_rho);
        double P = power_at_step(v, a, grade, rho, params);
        if (params.clamp_negative && P < 0.0) P = 0.0;

        pa.point_power_w[i] = P;
        sum      += P;
        energy_j += P * static_cast<double>(dt);
        if (P > max_p) max_p = P;
        ++count;

        if (pts[i].has_power) {
            const double meas = static_cast<double>(pts[i].power);
            meas_sum    += meas;
            abs_err_sum += std::fabs(P - meas);
            bias_sum    += (P - meas);
            ++meas_count;
        }
    }

    if (count > 0) {
        pa.stats.valid       = true;
        pa.stats.avg_power_w = sum / static_cast<double>(count);
        pa.stats.max_power_w = max_p;
        pa.stats.total_kj    = energy_j / 1000.0;
    }
    if (meas_count > 0) {
        pa.stats.has_measured   = true;
        pa.stats.avg_measured_w = meas_sum    / static_cast<double>(meas_count);
        pa.stats.mean_abs_err_w = abs_err_sum / static_cast<double>(meas_count);
        pa.stats.mean_bias_w    = bias_sum    / static_cast<double>(meas_count);
    }

    return pa;
}

// ---------------------------------------------------------------------------
// GpxReader::attach_climb_power
// ---------------------------------------------------------------------------

void GpxReader::attach_climb_power(std::vector<Hill>& hills,
                                   const PowerAnalysis& pa) const
{
    const std::size_t n = pa.point_power_w.size();
    if (n == 0) return;

    for (Hill& h : hills) {
        if (h.end_idx >= n || h.end_idx <= h.start_idx) continue;
        double      s = 0.0;
        std::size_t c = 0;
        // Average the step powers within the climb: indices (start_idx, end_idx]
        for (std::size_t k = h.start_idx + 1; k <= h.end_idx; ++k) {
            s += pa.point_power_w[k];
            ++c;
        }
        if (c > 0) {
            h.avg_power_w = s / static_cast<double>(c);
            h.has_power   = true;
        }
    }
}
