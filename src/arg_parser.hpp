#pragma once

/**
 * @file arg_parser.hpp
 * @brief Command-line parsing. parse() turns argv into a validated Options
 *        struct with defaults applied; main() then just consumes it.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // PowerParams
#include "wind.hpp"         // wind::Mode

#include <iosfwd>
#include <string>
#include <vector>

namespace arg_parser {

/** @brief Everything the program can be told to do, parsed from the command line. */
struct Options {
    std::vector<std::string> filepaths;    /**< Positional args: one or more .gpx files. */

    Size              max_print = 10;      /**< --points: track points to print. */
    std::vector<Real> dist_windows;        /**< --dist (km), repeatable. */
    std::vector<Long> time_windows;        /**< --time (s), repeatable. */

    PowerParams power;                      /**< Physics model (--mass/--crr/--cda/...). */

    // Rider profile used by the training metrics.
    Real        ftp_w        = 305.0;       /**< --ftp: functional threshold power (W). */
    Real        body_mass_kg = 71.3;        /**< --weight/--rider: body weight for W/kg (kg). */
    Real        lthr         = 0.0;         /**< --lthr: lactate-threshold HR (bpm); 0 = unset. */
    Real        max_hr       = 0.0;         /**< --max-hr: maximum HR (bpm); 0 = unset. */
    Real        crank_length_m = 0.1725;    /**< --crank: crank-arm length (m) for quadrants. */

    std::string power_csv;                  /**< --power-csv output path. */
    std::string xy_path;                    /**< --xy output path. */
    std::string power_curve_path;           /**< --power-curve output path. */
    std::string power_hist_path;            /**< --power-hist output path. */
    Real        hist_bin_w = 25.0;          /**< --hist-bin: histogram bin width (W). */
    Real        split_km   = 0.0;           /**< --splits: split length (km); 0 = off. */
    std::string wbal_path;                  /**< --wbal-file output path. */

    std::string acf_velocity;               /**< --acf-velocity output path. */
    std::string acf_power;                  /**< --acf-power output path. */
    std::string acf_power_measured;         /**< --acf-power-measured output path. */
    std::string acf_hr;                     /**< --acf-hr output path. */
    std::string acf_cadence;
    std::string acf_torque;         /**< --acf-torque output path. */                /**< --acf-cadence output path. */
    Real        acf_dt = 0.0;               /**< --acf-dt: resample interval (s); 0 = auto. */

    wind::Mode  wind_mode = wind::Mode::Off; /**< --wind / --wind-cache / --wind-file. */
    std::string wind_path;                   /**< Wind cache/file path. */
};

/**
 * @brief Parse argv[1..] into @p opts.
 * @param argc  Argument count from main().
 * @param argv  Argument vector from main().
 * @param opts  Out: the filled options (defaults applied, mass resolved).
 * @param error Out: a human-readable reason on failure (empty when the user
 *              simply supplied no arguments).
 * @return True on success; on false the caller should print usage.
 */
Bool parse(Int argc, Char* argv[], Options& opts, std::string& error);

/**
 * @brief Print the usage / help text.
 * @param prog Program name (argv[0]).
 * @param os   Destination stream.
 */
void print_usage(const Char* prog, std::ostream& os);

} // namespace arg_parser
