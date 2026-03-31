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

private:
    GpxData     data_;
    std::string error_;
};
