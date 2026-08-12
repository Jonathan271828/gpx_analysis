#pragma once

#include "types.hpp"
#include "gpx_reader.hpp"   // PowerParams
#include "wind.hpp"         // wind::Mode

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

/// Everything the program can be told to do, parsed from the command line.
struct Options {
    std::vector<std::string> filepaths;    // positional args: one or more .gpx files

    Size              max_print = 10;      // --points
    std::vector<Real> dist_windows;        // --dist  (km), repeatable
    std::vector<Long> time_windows;        // --time  (s),  repeatable

    PowerParams power;                      // --mass/--rider/--bike/--crr/--cda/...
                                            // (mass resolved into total_mass_kg)

    // Rider profile used by the training metrics.
    Real        ftp_w        = 305.0;       // --ftp   functional threshold power (W)
    Real        body_mass_kg = 71.3;        // --weight/--rider body weight for W/kg
    Real        lthr         = 0.0;         // --lthr  lactate-threshold HR (bpm); 0 = unset
    Real        max_hr       = 0.0;         // --max-hr maximum HR (bpm); 0 = unset

    std::string power_csv;                  // --power-csv
    std::string xy_path;                    // --xy
    std::string power_curve_path;           // --power-curve
    std::string power_hist_path;            // --power-hist
    Real        hist_bin_w = 25.0;          // --hist-bin
    Real        split_km   = 0.0;           // --splits (km); 0 = off
    std::string wbal_path;                  // --wbal-file

    std::string acf_velocity;               // --acf-velocity
    std::string acf_power;                  // --acf-power
    std::string acf_power_measured;         // --acf-power-measured
    std::string acf_hr;                     // --acf-hr
    std::string acf_cadence;                // --acf-cadence
    Real        acf_dt = 0.0;               // --acf-dt (0 = auto)

    wind::Mode  wind_mode = wind::Mode::Off; // --wind / --wind-cache / --wind-file
    std::string wind_path;
};

/// Parse argv[1..] into `opts`. Returns true on success. On failure returns
/// false and puts a human-readable reason in `error` (empty when the user
/// simply supplied no arguments); the caller should then print usage.
Bool parse(Int argc, Char* argv[], Options& opts, std::string& error);

/// Print the usage / help text for program `prog` to stream `os`.
void print_usage(const Char* prog, std::ostream& os);

} // namespace arg_parser
