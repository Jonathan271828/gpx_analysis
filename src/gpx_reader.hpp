#pragma once

/**
 * @file gpx_reader.hpp
 * @brief Core data model and the GpxReader that parses a GPX file and computes
 *        track statistics, hills, fastest segments and the power estimate.
 */

#include "types.hpp"

#include <ctime>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

/** @brief One recorded track point with its optional sensor fields. */
struct TrackPoint {
    Real        lat   = 0.0;      /**< Latitude in decimal degrees (WGS-84). */
    Real        lon   = 0.0;      /**< Longitude in decimal degrees (WGS-84). */
    Real        ele   = 0.0;      /**< Elevation in metres. */
    std::string time;             /**< ISO-8601 timestamp, verbatim from the file. */
    Real        atemp = 0.0;      /**< Air temperature in °C. */
    Bool        has_atemp = false;/**< True if @ref atemp was present. */
    Int         hr    = 0;        /**< Heart rate in bpm. */
    Bool        has_hr = false;   /**< True if @ref hr was present. */
    Int         cad   = 0;        /**< Cadence in rpm. */
    Bool        has_cad = false;  /**< True if @ref cad was present. */
    Int         power = 0;        /**< Measured power in watts. */
    Bool        has_power = false;/**< True if @ref power was present. */
};

/** @brief A single `<trk>`: a name, type and its ordered points. */
struct Track {
    std::string             name;   /**< Track name from `<name>`. */
    std::string             type;   /**< Activity type from `<type>`. */
    std::vector<TrackPoint> points; /**< Points in file (time) order. */
};

/** @brief The whole parsed file: metadata plus every track. */
struct GpxData {
    std::string        metadata_time;  /**< `<metadata><time>` value, if any. */
    std::vector<Track> tracks;         /**< All `<trk>` elements. */
};

// ---------------------------------------------------------------------------
// Statistics computed from a single Track
// ---------------------------------------------------------------------------

/** @brief Summary statistics for one track (see GpxReader::compute_stats). */
struct TrackStats {
    Size num_points       = 0;    /**< Number of points. */
    Real total_distance_m = 0.0;  /**< Total 2-D Haversine distance (m). */
    Real elevation_gain_m = 0.0;  /**< Sum of positive elevation deltas (m). */
    Real elevation_loss_m = 0.0;  /**< Sum of negative deltas as a positive value (m). */
    Real min_ele_m        = 0.0;  /**< Minimum elevation (m). */
    Real max_ele_m        = 0.0;  /**< Maximum elevation (m). */
    Long duration_s       = 0;    /**< Elapsed time first→last point (s). */
    Real avg_speed_kmh    = 0.0;  /**< Average speed (km/h). */
    Real avg_atemp        = 0.0;  /**< Average air temperature (°C). */
    Bool has_atemp        = false;/**< True if any temperature was present. */
    // Sensor statistics (averaged over all points that carry the field)
    Real avg_hr    = 0.0; Int min_hr    = 0; Int max_hr    = 0; /**< Heart-rate stats (bpm). */
    Bool has_hr    = false;                                     /**< True if HR present. */
    Real avg_cad   = 0.0; Int min_cad   = 0; Int max_cad   = 0; /**< Cadence stats (rpm). */
    Bool has_cad   = false;                                     /**< True if cadence present. */
    Real avg_power = 0.0; Int min_power = 0; Int max_power = 0; /**< Measured-power stats (W). */
    Bool has_power = false;                                     /**< True if power present. */
    // Gradient statistics (only steps with horiz. distance >= 1 m are counted)
    Real avg_climb_pct   = 0.0;   /**< Mean gradient of uphill steps (%). */
    Real avg_descent_pct = 0.0;   /**< Mean gradient of downhill steps (%, negative). */
};

// ---------------------------------------------------------------------------
// Hill detection result
// ---------------------------------------------------------------------------

/** @brief One detected climb (see GpxReader::detect_hills). */
struct Hill {
    Size start_idx     = 0;       /**< Index of the climb's first point. */
    Size end_idx       = 0;       /**< Index of the climb's last (peak) point. */
    Real distance_m    = 0.0;     /**< Horizontal distance of the climb (m). */
    Real gain_m        = 0.0;     /**< Elevation gained, end − start (m). */
    Real start_ele_m   = 0.0;     /**< Elevation at the start (m). */
    Real end_ele_m     = 0.0;     /**< Elevation at the top (m). */
    Real avg_grade_pct = 0.0;     /**< Average gradient, gain/distance × 100 (%). */
    std::string start_time;       /**< Timestamp at the start. */
    std::string end_time;         /**< Timestamp at the top. */
    Real avg_power_w   = 0.0;     /**< Mean estimated power over the climb (W). */
    Bool has_power     = false;   /**< True once attach_climb_power() filled it. */
    Long duration_s    = 0;       /**< Climb duration (s); 0 if timestamps unknown. */
    Real vam_mh        = 0.0;     /**< Vertical ascent metres per hour. */
    Real climb_score   = 0.0;     /**< distance_m × avg_grade_pct (Strava-style). */
    std::string category;         /**< "HC", "1".."4", or "" if below Cat 4. */
};

// ---------------------------------------------------------------------------
// Best segment result
// ---------------------------------------------------------------------------

/**
 * @brief Fastest segment over a distance or time window (see
 *        GpxReader::fastest_by_distance / GpxReader::fastest_by_time).
 */
struct BestSegment {
    Size start_idx     = 0;       /**< Index of the segment start. */
    Size end_idx       = 0;       /**< Index of the segment end. */
    Real distance_m    = 0.0;     /**< Distance covered (m). */
    Long duration_s    = 0;       /**< Duration (s). */
    Real avg_speed_kmh = 0.0;     /**< Average speed over the segment (km/h). */
    std::string start_time;       /**< Timestamp at the start. */
    std::string end_time;         /**< Timestamp at the end. */
    Real start_lat     = 0.0;     /**< Latitude at the start. */
    Real start_lon     = 0.0;     /**< Longitude at the start. */
    Real end_lat       = 0.0;     /**< Latitude at the end. */
    Real end_lon       = 0.0;     /**< Longitude at the end. */
    Bool valid         = false;   /**< False if the window is larger than the track. */
};

// ---------------------------------------------------------------------------
// Power estimation (Strava-style physics model)
// ---------------------------------------------------------------------------

/**
 * @brief Parameters of the physics-based power estimate.
 *
 * Mass is optional on the CLI (default rider + bike = 80 kg), so estimation
 * runs by default. The de-spiking limits guard against GPS/elevation noise
 * producing physically impossible power.
 */
struct PowerParams {
    Real total_mass_kg  = 80.0;   /**< Rider + bike + kit (kg). */
    Real crr            = 0.005;  /**< Rolling-resistance coefficient. */
    Real cda            = 0.32;   /**< Aerodynamic drag area CdA (m²). */
    Real drivetrain_eff = 0.977;  /**< Drivetrain efficiency (1 − loss). */
    Real default_rho    = 1.225;  /**< Fallback air density (kg/m³). */
    Bool clamp_negative = true;   /**< Clamp coasting/downhill power to 0 W. */
    Real smooth_window_s = 5.0;   /**< Centred speed smoothing window (s; 0 = off). */
    Real max_accel_ms2   = 3.0;   /**< Clamp on |acceleration| (m/s²). */
    Real max_speed_ms    = 30.0;  /**< Cap on raw step speed (m/s); drops GPS teleports. */
    Real max_grade       = 0.30;  /**< Clamp on |grade| (fraction). */
    Real max_gap_s       = 10.0;  /**< Steps longer than this count as a stop (0 W). */
};

/** @brief Summary of the estimated power (and its agreement with measured power). */
struct PowerStats {
    Bool valid          = false;  /**< True if power could be estimated. */
    Real avg_power_w     = 0.0;   /**< Mean estimated power over moving samples (W). */
    Real max_power_w     = 0.0;   /**< Peak estimated power (W). */
    Real total_kj        = 0.0;   /**< Work done, Σ(P·dt)/1000 (kJ). */
    Bool has_measured    = false; /**< True if the track carried `<power>`. */
    Real avg_measured_w  = 0.0;   /**< Mean measured power (W). */
    Real mean_abs_err_w  = 0.0;   /**< Mean |estimated − measured| (W). */
    Real mean_bias_w     = 0.0;   /**< Mean (estimated − measured) (W). */
    Bool has_wind        = false; /**< True if wind data was applied. */
    Real avg_headwind_ms = 0.0;   /**< Mean headwind component (m/s; + head, − tail). */
};

/**
 * @brief Full result of GpxReader::estimate_power: the summary plus per-point
 *        series (each sized to points.size(), index 0 == 0/sentinel).
 */
struct PowerAnalysis {
    PowerStats        stats;          /**< Summary statistics. */
    std::vector<Real> point_power_w;  /**< Estimated power on step (i-1 → i) (W). */
    std::vector<Long> t_offset_s;     /**< Seconds from the first point (−1 if unknown). */
    std::vector<Real> headwind_ms;    /**< Per-step headwind component (m/s). */
    std::vector<Real> cum_dist_m;     /**< Cumulative distance from the start (m). */
    std::vector<Real> speed_ms;       /**< Instantaneous ground speed on the step (m/s). */
};

/** @brief Mean-maximal power curve: best average power per duration. */
struct PowerCurve {
    Bool              valid        = false; /**< True if any duration was emitted. */
    Bool              has_measured = false; /**< True if a measured curve is included. */
    std::vector<Long> duration_s;           /**< Emitted durations, ascending (s). */
    std::vector<Real> est_power_w;          /**< Best average estimated power (W). */
    std::vector<Real> meas_power_w;         /**< Best average measured power (W). */
};

/** @brief Time (seconds) spent in each fixed-width power band. */
struct PowerHistogram {
    Bool              valid        = false; /**< True if the histogram was built. */
    Bool              has_measured = false; /**< True if measured bins are included. */
    Real              bin_w        = 25.0;  /**< Bin width (W). */
    std::vector<Real> bin_lo_w;             /**< Lower edge of each bin (W). */
    std::vector<Real> est_seconds;          /**< Time-in-bin for estimated power (s). */
    std::vector<Real> meas_seconds;         /**< Time-in-bin for measured power (s). */
};

// ---------------------------------------------------------------------------
// Wind data (plain data; produced by the wind module, consumed by the core)
// ---------------------------------------------------------------------------

/** @brief Hourly wind samples for a location, produced by the wind module. */
struct WindData {
    std::vector<Time> times;     /**< Hourly UTC timestamps (ascending). */
    std::vector<Real> speed_ms;  /**< Wind speed at 10 m (m/s). */
    std::vector<Real> dir_deg;   /**< Wind FROM direction (deg, meteorological). */
    Bool              valid = false; /**< True if the data is usable. */

    /**
     * @brief Nearest-hour lookup.
     * @param t     Query time (UTC).
     * @param speed Out: wind speed at the nearest hour (m/s).
     * @param dir   Out: wind direction at the nearest hour (deg).
     * @return True on success; false if empty or `t` is far outside the range.
     */
    Bool sample(Time t, Real& speed, Real& dir) const;
};

// ---------------------------------------------------------------------------
// GpxReader class
// ---------------------------------------------------------------------------

/** @brief Parses a GPX file and computes all track-level analyses. */
class GpxReader {
public:
    /**
     * @brief Parse the file at the given path.
     * @param filepath Path to a GPX 1.1 file.
     * @return True on success; on failure returns false and sets error_message().
     */
    Bool parse(const std::string& filepath);

    /** @return The parsed data (valid after a successful parse()). */
    const GpxData&    data()          const { return data_; }
    /** @return A human-readable description of the last parse failure. */
    const std::string& error_message() const { return error_; }

    /**
     * @brief Compute summary statistics for a track.
     * @param track_index Index into data().tracks.
     * @return The statistics, or a zero-initialised value for a bad index.
     */
    TrackStats compute_stats(Size track_index = 0) const;

    /**
     * @brief Find the fastest segment of at least `window_m` metres.
     * @param window_m    Minimum window length (m).
     * @param track_index Index into data().tracks.
     * @return The fastest segment; `.valid` is false if the track is shorter.
     */
    BestSegment fastest_by_distance(Real window_m,
                                    Size track_index = 0) const;

    /**
     * @brief Find the fastest segment of at least `window_s` seconds.
     * @param window_s    Minimum window length (s).
     * @param track_index Index into data().tracks.
     * @return The fastest segment; `.valid` is false if the track is shorter.
     */
    BestSegment fastest_by_time(Long window_s,
                                Size track_index = 0) const;

    /**
     * @brief Detect individual climbs in the track.
     *
     * A hill requires a minimum step grade and total gain; short flat/downhill
     * gaps are absorbed. Each result carries VAM and a climb category.
     * @param track_index Index into data().tracks.
     * @return The detected hills in order.
     */
    std::vector<Hill> detect_hills(Size track_index = 0) const;

    /**
     * @brief Estimate power along the track using the physics model.
     * @param params      Model parameters (mass, CdA, Crr, de-spiking limits).
     * @param track_index Index into data().tracks.
     * @param wind        Optional wind data; when non-null and valid the aero
     *                    term uses the real headwind component.
     * @return The summary plus per-point series.
     */
    PowerAnalysis estimate_power(const PowerParams& params,
                                 Size track_index = 0,
                                 const WindData* wind = nullptr) const;

    /**
     * @brief Fill each hill's avg_power_w from the per-point power series.
     * @param hills In/out: hills to annotate (indices must match `pa`).
     * @param pa    Power analysis for the same track.
     */
    void attach_climb_power(std::vector<Hill>& hills,
                            const PowerAnalysis& pa) const;

    /**
     * @brief Mean-maximal power curve: best time-weighted average power over any
     *        window of each requested duration.
     * @param pa          Power analysis for the track.
     * @param durations_s Window lengths to evaluate (s); longer-than-ride skipped.
     * @param track_index Index into data().tracks.
     * @return The curve (estimated, plus measured when the track carries power).
     */
    PowerCurve power_curve(const PowerAnalysis& pa,
                           const std::vector<Long>& durations_s,
                           Size track_index = 0) const;

    /**
     * @brief Power histogram: seconds spent in each `bin_w`-wide power band.
     * @param pa          Power analysis for the track.
     * @param bin_w       Bin width (W).
     * @param track_index Index into data().tracks.
     * @return The histogram (estimated, plus measured when present).
     */
    PowerHistogram power_histogram(const PowerAnalysis& pa,
                                   Real bin_w,
                                   Size track_index = 0) const;

private:
    GpxData     data_;   /**< Parsed file contents. */
    std::string error_;  /**< Last parse-error message. */
};
