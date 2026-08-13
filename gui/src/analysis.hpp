#pragma once

/// @file analysis.hpp
/// @brief Runs the existing command-line analysis and captures its report.
///
/// The GUI does not reimplement any analysis. It builds the same options struct
/// the command line produces, calls app::run(), and captures what that writes
/// to std::cout / std::cerr. Any report section added to the analysis code
/// therefore shows up in the GUI with no change here.

#include "channels.hpp"   // channels::Channel
#include "zones.hpp"      // zones::ZoneTable

#include <cstddef>
#include <string>
#include <vector>

namespace gui {

/// @brief A stretch of a climb over which the gradient is roughly constant.
///
/// Indices refer into the parent HillProfile's sample arrays, and consecutive
/// segments share their boundary sample so the shaded fills meet without a gap.
struct HillSegment {
    std::size_t begin         = 0;     ///< First sample of the stretch.
    std::size_t end           = 0;     ///< Last sample, inclusive.
    Real        dist_m        = 0.0;   ///< Horizontal length (m).
    Real        avg_grade_pct = 0.0;   ///< Gradient over the stretch (%).
    Real        avg_power_w   = 0.0;   ///< Mean estimated power (W).
    bool        has_power     = false; ///< Whether @ref avg_power_w is usable.
    int         zone          = -1;    ///< Power zone index, or -1 if unknown.
};

/// @brief One climb's elevation profile, plus the numbers naming it.
struct HillProfile {
    Real        distance_km   = 0.0;   ///< Horizontal length of the climb (km).
    Real        gain_m        = 0.0;   ///< Elevation gained (m).
    Real        avg_grade_pct = 0.0;   ///< Average gradient (%).
    Real        vam_mh        = 0.0;   ///< Vertical ascent metres per hour.
    Real        avg_power_w   = 0.0;   ///< Mean estimated power (W).
    bool        has_power     = false; ///< Whether @ref avg_power_w is set.
    Long        duration_s    = 0;     ///< Climb duration (s); 0 if unknown.
    std::string category;              ///< "HC", "1".."4", or empty.
    std::string start_time;            ///< Timestamp at the foot of the climb.

    /// Profile samples from the foot of the climb to its top. @ref ele_m is the
    /// curve; the two x options are the same points measured two ways.
    /// @ref time_min is empty when the climb has no usable timestamps.
    std::vector<Real> dist_km;
    std::vector<Real> time_min;
    std::vector<Real> ele_m;

    /// Estimated power at each sample (W), same length as @ref ele_m.
    std::vector<Real> power_w;

    /// The climb split into stretches of roughly constant gradient, covering it
    /// end to end. Empty when the profile has too few samples.
    std::vector<HillSegment> segments;
};

/// @brief One entry of the mean-maximal power curve: the best average power the
/// ride held for a given duration.
struct PeakBar {
    Long duration_s     = 0;      ///< Window length (s).
    Real avg_power_w    = 0.0;    ///< Best average power over that window (W).
    Real wkg            = 0.0;    ///< The same, per kilogram of body mass.
    Long start_offset_s = 0;      ///< Seconds from the ride start to the window.
    int  zone           = -1;     ///< Power zone index, or -1 if unknown.
    bool measured       = false;  ///< True when taken from a power meter.
};

/// @brief Everything chartable about one track.
struct TrackCharts {
    zones::ZoneTable         power_zones;   ///< Time-in-zone distribution.
    std::vector<HillProfile> hills;         ///< One entry per detected climb.
    std::vector<PeakBar>     peaks;         ///< Mean-maximal power curve.
    Real                     ftp_w = 0.0;   ///< FTP the analysis ran with (W).

    /// The per-sample series the track carries (velocity, power, heart rate,
    /// cadence), ready to hand to signal::compute_acf_psd().
    std::vector<channels::Channel> channels;
};

/// @brief The captured result of analysing one GPX file.
struct Result {
    bool        ok = false;   ///< True when the analysis reported success.
    std::string summary;      ///< Everything written to stdout (the report).
    std::string errors;       ///< Everything written to stderr (warnings/errors).

    /// Chartable data per track. The text report above already contains these
    /// as tables; this is the same data in structured form so it can be drawn.
    std::vector<TrackCharts> tracks;
};

/// @brief Where the analysis should get wind data, mirroring the CLI's
/// --wind-cache / --wind-file. Without wind the aero term carries no headwind,
/// which changes estimated power and everything derived from it.
///
/// The CLI's plain --wind (fetch every run) is deliberately not offered: the
/// report and the charts each acquire wind once, so a non-caching fetch would
/// hit the network twice per load.
enum class WindSource {
    Off,    ///< No wind applied (the default).
    Cache,  ///< Fetch from Open-Meteo once, cache to disk, reuse thereafter.
    File    ///< Read a local Open-Meteo JSON file; never touches the network.
};

/// @brief Analyse one GPX file and capture the report it prints.
/// @param gpx_path  Path to the .gpx file to analyse.
/// @param max_print Track points to list first (0 suppresses that section).
/// @param wind      Where to source wind data.
/// @param wind_path Cache/file path used when @p wind is not Off.
/// @return The captured report. With @p wind Off no files are written and no
///         network request is made. Cache writes @p wind_path and may fetch
///         from Open-Meteo; File only reads.
Result analyse(const std::string& gpx_path, std::size_t max_print,
               WindSource wind = WindSource::Off,
               const std::string& wind_path = std::string());

} // namespace gui
