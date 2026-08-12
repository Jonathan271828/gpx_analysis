#include "arg_parser.hpp"

#include <cstdlib>
#include <ostream>
#include <string>

namespace arg_parser {

// ---------------------------------------------------------------------------
// print_usage
// ---------------------------------------------------------------------------

void print_usage(const Char* prog, std::ostream& os) {
    os << "Usage: " << prog << " <file.gpx> [options]\n\n"
       << "Options:\n"
       << "  --points N       print first N track points (default: 10)\n"
       << "  --dist  D        find fastest segment of D km  (e.g. --dist 5.0)\n"
       << "  --time  T        find fastest segment of T seconds (e.g. --time 300)\n"
       << "\nPower estimation (Strava-style physics model, runs by default):\n"
       << "  --mass  M        total rider+bike mass in kg (default: 80)\n"
       << "  --rider R        rider mass in kg (summed with --bike if --mass unset)\n"
       << "  --bike  B        bike mass in kg (summed with --rider if --mass unset)\n"
       << "  --crr   C        rolling resistance coefficient (default: 0.005)\n"
       << "  --cda   A        aerodynamic drag area CdA in m^2 (default: 0.32)\n"
       << "  --drivetrain E   drivetrain efficiency 0..1 (default: 0.977)\n"
       << "  --smooth S       GPS speed smoothing window in s, tames spikes (default: 5; 0 off)\n"
       << "  --max-accel A    clamp on |acceleration| in m/s^2 (default: 3)\n"
       << "  --max-speed V    cap on raw step speed in m/s, drops GPS teleports (default: 30)\n"
       << "  --max-grade G    clamp on |grade| as a fraction (default: 0.30)\n"
       << "  --max-gap S      steps longer than S seconds count as a stop (default: 10)\n"
       << "  --power-csv F    write a time-vs-power CSV to file F\n"
       << "  --xy F           write all per-point data as a #-commented XY table to F\n"
       << "  --power-curve F  write mean-maximal power curve (duration vs W) to F\n"
       << "  --power-hist F   write power histogram (time in each power band) to F\n"
       << "  --hist-bin W     histogram bin width in watts (default: 25)\n"
       << "\nAutocorrelation & power spectrum (4-col: lag, acf, freq, psd):\n"
       << "  --acf-velocity F       velocity autocorrelation + spectrum -> F\n"
       << "  --acf-power F          estimated power                     -> F\n"
       << "  --acf-power-measured F measured <power>                    -> F\n"
       << "  --acf-hr F             heart rate                          -> F\n"
       << "  --acf-cadence F        cadence (rpm)                       -> F\n"
       << "  --acf-dt S             uniform resample interval in s (default: auto = median)\n"
       << "\nWind (Open-Meteo historical API; improves the aero term):\n"
       << "  --wind           fetch historical wind and apply it\n"
       << "  --wind-cache F   like --wind, but cache to/read from file F\n"
       << "  --wind-file F    apply wind from local JSON file F (offline)\n"
       << "\nMultiple --dist and --time flags are supported.\n";
}

// ---------------------------------------------------------------------------
// parse
// ---------------------------------------------------------------------------

Bool parse(Int argc, Char* argv[], Options& opts, std::string& error) {
    error.clear();
    if (argc < 2) return false;             // no input file → caller shows usage

    opts.filepath = argv[1];

    // Mass is optional (default rider+bike = 80 kg). Collect the pieces locally
    // and resolve the total after the loop.
    Real rider_kg   = 71.0;
    Real bike_kg    = 9.0;
    Bool mass_set   = false;
    Real mass_total = 80.0;

    for (Int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--points" && i + 1 < argc) {
            opts.max_print = static_cast<Size>(std::atoi(argv[++i]));
        } else if (arg == "--dist" && i + 1 < argc) {
            opts.dist_windows.push_back(std::atof(argv[++i]));
        } else if (arg == "--time" && i + 1 < argc) {
            opts.time_windows.push_back(static_cast<Long>(std::atol(argv[++i])));
        } else if (arg == "--mass" && i + 1 < argc) {
            mass_total = std::atof(argv[++i]); mass_set = true;
        } else if (arg == "--rider" && i + 1 < argc) {
            rider_kg = std::atof(argv[++i]);
        } else if (arg == "--bike" && i + 1 < argc) {
            bike_kg = std::atof(argv[++i]);
        } else if (arg == "--crr" && i + 1 < argc) {
            opts.power.crr = std::atof(argv[++i]);
        } else if (arg == "--cda" && i + 1 < argc) {
            opts.power.cda = std::atof(argv[++i]);
        } else if (arg == "--drivetrain" && i + 1 < argc) {
            opts.power.drivetrain_eff = std::atof(argv[++i]);
        } else if (arg == "--smooth" && i + 1 < argc) {
            opts.power.smooth_window_s = std::atof(argv[++i]);
        } else if (arg == "--max-accel" && i + 1 < argc) {
            opts.power.max_accel_ms2 = std::atof(argv[++i]);
        } else if (arg == "--max-speed" && i + 1 < argc) {
            opts.power.max_speed_ms = std::atof(argv[++i]);
        } else if (arg == "--max-grade" && i + 1 < argc) {
            opts.power.max_grade = std::atof(argv[++i]);
        } else if (arg == "--max-gap" && i + 1 < argc) {
            opts.power.max_gap_s = std::atof(argv[++i]);
        } else if (arg == "--power-csv" && i + 1 < argc) {
            opts.power_csv = argv[++i];
        } else if (arg == "--xy" && i + 1 < argc) {
            opts.xy_path = argv[++i];
        } else if (arg == "--power-curve" && i + 1 < argc) {
            opts.power_curve_path = argv[++i];
        } else if (arg == "--power-hist" && i + 1 < argc) {
            opts.power_hist_path = argv[++i];
        } else if (arg == "--hist-bin" && i + 1 < argc) {
            opts.hist_bin_w = std::atof(argv[++i]);
        } else if (arg == "--acf-velocity" && i + 1 < argc) {
            opts.acf_velocity = argv[++i];
        } else if (arg == "--acf-power" && i + 1 < argc) {
            opts.acf_power = argv[++i];
        } else if (arg == "--acf-power-measured" && i + 1 < argc) {
            opts.acf_power_measured = argv[++i];
        } else if (arg == "--acf-hr" && i + 1 < argc) {
            opts.acf_hr = argv[++i];
        } else if (arg == "--acf-cadence" && i + 1 < argc) {
            opts.acf_cadence = argv[++i];
        } else if (arg == "--acf-dt" && i + 1 < argc) {
            opts.acf_dt = std::atof(argv[++i]);
        } else if (arg == "--wind") {
            opts.wind_mode = WindMode::Fetch;
        } else if (arg == "--wind-cache" && i + 1 < argc) {
            opts.wind_mode = WindMode::Cache; opts.wind_path = argv[++i];
        } else if (arg == "--wind-file" && i + 1 < argc) {
            opts.wind_mode = WindMode::File; opts.wind_path = argv[++i];
        } else {
            error = "Unknown option: " + arg;
            return false;
        }
    }

    // --mass sets the total directly; otherwise sum rider + bike (default 80 kg).
    opts.power.total_mass_kg = mass_set ? mass_total : (rider_kg + bike_kg);
    return true;
}

} // namespace arg_parser
