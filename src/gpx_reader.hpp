#pragma once

#include "types.hpp"

#include <ctime>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct TrackPoint {
    Real      lat   = 0.0;
    Real      lon   = 0.0;
    Real      ele   = 0.0;   // metres
    std::string time;           // ISO-8601 string as-is from the file
    Real      atemp = 0.0;   // air temperature in °C (from ns3:atemp)
    Bool        has_atemp = false;
    Int         hr    = 0;     // heart rate in bpm (from ns3:hr)
    Bool        has_hr = false;
    Int         cad   = 0;     // cadence in rpm (from ns3:cad)
    Bool        has_cad = false;
    Int         power = 0;     // power in watts (from <power>)
    Bool        has_power = false;
};

struct Track {
    std::string            name;
    std::string            type;
    std::vector<TrackPoint> points;
};

struct GpxData {
    std::string        metadata_time;
    std::vector<Track> tracks;
};

// ---------------------------------------------------------------------------
// Statistics computed from a single Track
// ---------------------------------------------------------------------------

struct TrackStats {
    Size num_points       = 0;
    Real      total_distance_m = 0.0;  // metres (Haversine)
    Real      elevation_gain_m = 0.0;  // sum of positive deltas
    Real      elevation_loss_m = 0.0;  // sum of negative deltas (positive value)
    Real      min_ele_m        = 0.0;
    Real      max_ele_m        = 0.0;
    Long        duration_s       = 0;    // seconds from first to last point
    Real      avg_speed_kmh    = 0.0;
    Real      avg_atemp        = 0.0;  // average air temperature
    Bool        has_atemp        = false;
    // Sensor statistics (averaged over all points that carry the field)
    Real      avg_hr           = 0.0;  Int min_hr    = 0;  Int max_hr    = 0;
    Bool        has_hr           = false;
    Real      avg_cad          = 0.0;  Int min_cad   = 0;  Int max_cad   = 0;
    Bool        has_cad          = false;
    Real      avg_power        = 0.0;  Int min_power = 0;  Int max_power = 0;
    Bool        has_power        = false;
    // Gradient statistics (only steps with horiz. distance >= 1 m are counted)
    Real      avg_climb_pct    = 0.0;  // mean gradient of uphill steps (%)
    Real      avg_descent_pct  = 0.0;  // mean gradient of downhill steps (%, negative)
};

// ---------------------------------------------------------------------------
// Hill detection result
// ---------------------------------------------------------------------------

struct Hill {
    Size start_idx     = 0;
    Size end_idx       = 0;
    Real      distance_m    = 0.0;   // horizontal distance of the climb
    Real      gain_m        = 0.0;   // elevation gained (end_ele - start_ele)
    Real      start_ele_m   = 0.0;
    Real      end_ele_m     = 0.0;
    Real      avg_grade_pct = 0.0;   // gain_m / distance_m * 100
    std::string start_time;
    std::string end_time;
    Real      avg_power_w   = 0.0;   // mean estimated power over the climb (W)
    Bool        has_power     = false; // true once attach_climb_power() fills it
};

// ---------------------------------------------------------------------------
// Best segment result
// ---------------------------------------------------------------------------

struct BestSegment {
    Size start_idx     = 0;
    Size end_idx       = 0;
    Real      distance_m    = 0.0;   // actual distance covered
    Long        duration_s    = 0;     // actual duration in seconds
    Real      avg_speed_kmh = 0.0;
    std::string start_time;
    std::string end_time;
    Real      start_lat     = 0.0;
    Real      start_lon     = 0.0;
    Real      end_lat       = 0.0;
    Real      end_lon       = 0.0;
    Bool        valid         = false; // false if window larger than track
};

// ---------------------------------------------------------------------------
// Power estimation (Strava-style physics model)
// ---------------------------------------------------------------------------

struct PowerParams {
    // Mass is optional on the CLI: default rider + bike = 80 kg total, so
    // estimation runs by default. Flags override any of these.
    Real total_mass_kg  = 80.0;   // rider + bike + kit
    Real crr            = 0.005;  // rolling resistance coefficient (road tyre)
    Real cda            = 0.32;   // drag area CdA in m^2 (rider on hoods)
    Real drivetrain_eff = 0.977;  // 1 - drivetrain loss (~2.3%)
    Real default_rho    = 1.225;  // fallback air density if no temp/elevation
    Bool   clamp_negative = true;   // clamp coasting/downhill power to 0 W
    // GPS speed is noisy: a few metres of position error on a 1 s step implies a
    // large spurious acceleration, which the m*a term turns into impossible
    // power spikes. Smooth the speed over this window (s; 0 disables) and cap the
    // acceleration as a backstop before evaluating the model.
    Real smooth_window_s = 5.0;   // centred moving-average window for speed
    Real max_accel_ms2   = 3.0;   // clamp on |acceleration| (m/s^2)
    Real max_speed_ms    = 30.0;  // cap on raw step speed (~108 km/h); a GPS
                                    // teleport above this is noise, not a descent
    Real max_grade       = 0.30;  // clamp on |grade|; elevation drift over a
                                    // short step can imply an impossible slope
    Real max_gap_s       = 10.0;  // steps longer than this are treated as a
                                    // stop/dropout (0 W); GPS drift during a long
                                    // pause otherwise fakes a big climb
};

struct PowerStats {
    Bool   valid          = false;
    Real avg_power_w     = 0.0;   // mean estimated power over moving samples
    Real max_power_w     = 0.0;
    Real total_kj        = 0.0;   // work done = sum(P * dt) / 1000
    Bool   has_measured    = false; // set if the track carried <power>
    Real avg_measured_w  = 0.0;
    Real mean_abs_err_w  = 0.0;   // mean |estimated - measured|
    Real mean_bias_w     = 0.0;   // mean (estimated - measured)
    Bool   has_wind        = false; // set if wind data was applied
    Real avg_headwind_ms = 0.0;   // mean headwind component (m/s; +head, -tail)
};

// Full result of estimate_power(): the summary plus the per-point series that
// drives the hills-table column and the time-vs-power CSV.
struct PowerAnalysis {
    PowerStats          stats;
    std::vector<Real> point_power_w;  // size == points.size(); index 0 == 0.0,
                                        // index i = est. power on step (i-1 -> i)
    std::vector<Long>   t_offset_s;     // size == points.size(); seconds from the
                                        // first point (-1 if timestamp unknown)
    std::vector<Real> headwind_ms;    // size == points.size(); per-step headwind
                                        // component (m/s), 0 if no wind applied
    std::vector<Real> cum_dist_m;     // size == points.size(); cumulative distance
                                        // from the first point (m); index 0 == 0.0
    std::vector<Real> speed_ms;       // size == points.size(); instantaneous ground
                                        // speed on step (i-1 -> i); index 0 == 0.0
};

// Mean-maximal power curve (Strava-style): for each duration, the best average
// power sustained over any window of that length in the ride.
struct PowerCurve {
    Bool                valid        = false;
    Bool                has_measured = false;
    std::vector<Long>   duration_s;      // durations actually emitted (ascending)
    std::vector<Real> est_power_w;     // best avg estimated power per duration
    std::vector<Real> meas_power_w;    // best avg measured power (if has_measured)
};

// Power histogram: time (seconds) spent in each fixed-width power band.
struct PowerHistogram {
    Bool                valid        = false;
    Bool                has_measured = false;
    Real              bin_w        = 25.0;  // bin width in watts
    std::vector<Real> bin_lo_w;            // lower edge of each bin (W)
    std::vector<Real> est_seconds;         // time-in-bin for estimated power
    std::vector<Real> meas_seconds;        // time-in-bin for measured power (if any)
};

// ---------------------------------------------------------------------------
// Wind data (plain data; produced by the wind module, consumed by the core)
// ---------------------------------------------------------------------------

struct WindData {
    std::vector<Time> times;     // hourly UTC timestamps (sorted ascending)
    std::vector<Real>      speed_ms;  // wind speed at 10 m (m/s)
    std::vector<Real>      dir_deg;   // wind FROM direction (deg, meteorological)
    Bool                     valid = false;

    /// Nearest-hour lookup. Sets speed/dir and returns true, or returns false
    /// if empty or `t` lies far outside the covered range.
    Bool sample(Time t, Real& speed, Real& dir) const;
};

// ---------------------------------------------------------------------------
// GpxReader class
// ---------------------------------------------------------------------------

class GpxReader {
public:
    /// Parse the file at the given path.
    /// Returns true on success; sets error_message() on failure.
    Bool parse(const std::string& filepath);

    const GpxData&    data()          const { return data_; }
    const std::string& error_message() const { return error_; }

    /// Compute statistics for the given track index.
    TrackStats compute_stats(Size track_index = 0) const;

    /// Find the fastest segment of at least window_m metres (sliding window).
    BestSegment fastest_by_distance(Real window_m,
                                    Size track_index = 0) const;

    /// Find the fastest segment of at least window_s seconds (sliding window).
    BestSegment fastest_by_time(Long window_s,
                                Size track_index = 0) const;

    /// Detect individual climbs in the track.
    /// A hill requires grade >= MIN_GRADE_PCT and total gain >= MIN_GAIN_M.
    /// Short flat/downhill gaps up to GAP_TOLERANCE_M are absorbed.
    std::vector<Hill> detect_hills(Size track_index = 0) const;

    /// Estimate power along the track using the physics model in PowerParams.
    /// Returns a summary plus a per-point power series. When `wind` is non-null
    /// and valid, the aerodynamic term uses the real headwind component.
    PowerAnalysis estimate_power(const PowerParams& params,
                                 Size track_index = 0,
                                 const WindData* wind = nullptr) const;

    /// Fill each hill's avg_power_w by averaging the per-point power series
    /// over the climb's index range. Safe to call with any hills/analysis.
    void attach_climb_power(std::vector<Hill>& hills,
                            const PowerAnalysis& pa) const;

    /// Mean-maximal power curve: for each requested duration (seconds), the best
    /// time-weighted average power over any window of that length. Durations
    /// longer than the ride are skipped. Uses the estimated series in `pa`, plus
    /// the measured <power> series when the track carries it.
    PowerCurve power_curve(const PowerAnalysis& pa,
                           const std::vector<Long>& durations_s,
                           Size track_index = 0) const;

    /// Power histogram: seconds spent in each `bin_w`-wide power band, for the
    /// estimated series (and the measured series when present).
    PowerHistogram power_histogram(const PowerAnalysis& pa,
                                   Real bin_w,
                                   Size track_index = 0) const;

private:
    GpxData     data_;
    std::string error_;
};
