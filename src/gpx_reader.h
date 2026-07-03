#pragma once

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct TrackPoint {
    double      lat   = 0.0;
    double      lon   = 0.0;
    double      ele   = 0.0;   // metres
    std::string time;           // ISO-8601 string as-is from the file
    double      atemp = 0.0;   // air temperature in °C (from ns3:atemp)
    bool        has_atemp = false;
    int         hr    = 0;     // heart rate in bpm (from ns3:hr)
    bool        has_hr = false;
    int         cad   = 0;     // cadence in rpm (from ns3:cad)
    bool        has_cad = false;
    int         power = 0;     // power in watts (from <power>)
    bool        has_power = false;
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
    std::size_t num_points       = 0;
    double      total_distance_m = 0.0;  // metres (Haversine)
    double      elevation_gain_m = 0.0;  // sum of positive deltas
    double      elevation_loss_m = 0.0;  // sum of negative deltas (positive value)
    double      min_ele_m        = 0.0;
    double      max_ele_m        = 0.0;
    long        duration_s       = 0;    // seconds from first to last point
    double      avg_speed_kmh    = 0.0;
    double      avg_atemp        = 0.0;  // average air temperature
    bool        has_atemp        = false;
    // Sensor statistics (averaged over all points that carry the field)
    double      avg_hr           = 0.0;  int min_hr    = 0;  int max_hr    = 0;
    bool        has_hr           = false;
    double      avg_cad          = 0.0;  int min_cad   = 0;  int max_cad   = 0;
    bool        has_cad          = false;
    double      avg_power        = 0.0;  int min_power = 0;  int max_power = 0;
    bool        has_power        = false;
    // Gradient statistics (only steps with horiz. distance >= 1 m are counted)
    double      avg_climb_pct    = 0.0;  // mean gradient of uphill steps (%)
    double      avg_descent_pct  = 0.0;  // mean gradient of downhill steps (%, negative)
};

// ---------------------------------------------------------------------------
// Hill detection result
// ---------------------------------------------------------------------------

struct Hill {
    std::size_t start_idx     = 0;
    std::size_t end_idx       = 0;
    double      distance_m    = 0.0;   // horizontal distance of the climb
    double      gain_m        = 0.0;   // elevation gained (end_ele - start_ele)
    double      start_ele_m   = 0.0;
    double      end_ele_m     = 0.0;
    double      avg_grade_pct = 0.0;   // gain_m / distance_m * 100
    std::string start_time;
    std::string end_time;
    double      avg_power_w   = 0.0;   // mean estimated power over the climb (W)
    bool        has_power     = false; // true once attach_climb_power() fills it
};

// ---------------------------------------------------------------------------
// Best segment result
// ---------------------------------------------------------------------------

struct BestSegment {
    std::size_t start_idx     = 0;
    std::size_t end_idx       = 0;
    double      distance_m    = 0.0;   // actual distance covered
    long        duration_s    = 0;     // actual duration in seconds
    double      avg_speed_kmh = 0.0;
    std::string start_time;
    std::string end_time;
    double      start_lat     = 0.0;
    double      start_lon     = 0.0;
    double      end_lat       = 0.0;
    double      end_lon       = 0.0;
    bool        valid         = false; // false if window larger than track
};

// ---------------------------------------------------------------------------
// Power estimation (Strava-style physics model)
// ---------------------------------------------------------------------------

struct PowerParams {
    // Mass is optional on the CLI: default rider + bike = 80 kg total, so
    // estimation runs by default. Flags override any of these.
    double total_mass_kg  = 80.0;   // rider + bike + kit
    double crr            = 0.005;  // rolling resistance coefficient (road tyre)
    double cda            = 0.32;   // drag area CdA in m^2 (rider on hoods)
    double drivetrain_eff = 0.977;  // 1 - drivetrain loss (~2.3%)
    double default_rho    = 1.225;  // fallback air density if no temp/elevation
    bool   clamp_negative = true;   // clamp coasting/downhill power to 0 W
};

struct PowerStats {
    bool   valid          = false;
    double avg_power_w     = 0.0;   // mean estimated power over moving samples
    double max_power_w     = 0.0;
    double total_kj        = 0.0;   // work done = sum(P * dt) / 1000
    bool   has_measured    = false; // set if the track carried <power>
    double avg_measured_w  = 0.0;
    double mean_abs_err_w  = 0.0;   // mean |estimated - measured|
    double mean_bias_w     = 0.0;   // mean (estimated - measured)
};

// Full result of estimate_power(): the summary plus the per-point series that
// drives the hills-table column and the time-vs-power CSV.
struct PowerAnalysis {
    PowerStats          stats;
    std::vector<double> point_power_w;  // size == points.size(); index 0 == 0.0,
                                        // index i = est. power on step (i-1 -> i)
    std::vector<long>   t_offset_s;     // size == points.size(); seconds from the
                                        // first point (-1 if timestamp unknown)
};

// ---------------------------------------------------------------------------
// GpxReader class
// ---------------------------------------------------------------------------

class GpxReader {
public:
    /// Parse the file at the given path.
    /// Returns true on success; sets error_message() on failure.
    bool parse(const std::string& filepath);

    const GpxData&    data()          const { return data_; }
    const std::string& error_message() const { return error_; }

    /// Compute statistics for the given track index.
    TrackStats compute_stats(std::size_t track_index = 0) const;

    /// Find the fastest segment of at least window_m metres (sliding window).
    BestSegment fastest_by_distance(double window_m,
                                    std::size_t track_index = 0) const;

    /// Find the fastest segment of at least window_s seconds (sliding window).
    BestSegment fastest_by_time(long window_s,
                                std::size_t track_index = 0) const;

    /// Detect individual climbs in the track.
    /// A hill requires grade >= MIN_GRADE_PCT and total gain >= MIN_GAIN_M.
    /// Short flat/downhill gaps up to GAP_TOLERANCE_M are absorbed.
    std::vector<Hill> detect_hills(std::size_t track_index = 0) const;

    /// Estimate power along the track using the physics model in PowerParams.
    /// Returns a summary plus a per-point power series.
    PowerAnalysis estimate_power(const PowerParams& params,
                                 std::size_t track_index = 0) const;

    /// Fill each hill's avg_power_w by averaging the per-point power series
    /// over the climb's index range. Safe to call with any hills/analysis.
    void attach_climb_power(std::vector<Hill>& hills,
                            const PowerAnalysis& pa) const;

private:
    GpxData     data_;
    std::string error_;
};
