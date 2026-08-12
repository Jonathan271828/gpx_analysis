#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // PowerParams

#include <iosfwd>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Command-line argument parsing
//
// All CLI handling lives here, out of main(). parse() turns argv into a fully
// validated Options struct with defaults applied; main() then just consumes it.
// ---------------------------------------------------------------------------

namespace arg_parser {

/// Where wind data comes from (chosen by --wind / --wind-cache / --wind-file).
enum class WindMode { Off, Fetch, Cache, File };

/// Everything the program can be told to do, parsed from the command line.
struct Options {
    std::string filepath;                  // positional argument: the .gpx file

    Size              max_print = 10;      // --points
    std::vector<Real> dist_windows;        // --dist  (km), repeatable
    std::vector<Long> time_windows;        // --time  (s),  repeatable

    PowerParams power;                      // --mass/--rider/--bike/--crr/--cda/...
                                            // (mass resolved into total_mass_kg)

    std::string power_csv;                  // --power-csv
    std::string xy_path;                    // --xy
    std::string power_curve_path;           // --power-curve
    std::string power_hist_path;            // --power-hist
    Real        hist_bin_w = 25.0;          // --hist-bin

    std::string acf_velocity;               // --acf-velocity
    std::string acf_power;                  // --acf-power
    std::string acf_power_measured;         // --acf-power-measured
    std::string acf_hr;                     // --acf-hr
    std::string acf_cadence;                // --acf-cadence
    Real        acf_dt = 0.0;               // --acf-dt (0 = auto)

    WindMode    wind_mode = WindMode::Off;  // --wind / --wind-cache / --wind-file
    std::string wind_path;
};

/// Parse argv[1..] into `opts`. Returns true on success. On failure returns
/// false and puts a human-readable reason in `error` (empty when the user
/// simply supplied no arguments); the caller should then print usage.
Bool parse(Int argc, Char* argv[], Options& opts, std::string& error);

/// Print the usage / help text for program `prog` to stream `os`.
void print_usage(const Char* prog, std::ostream& os);

} // namespace arg_parser
