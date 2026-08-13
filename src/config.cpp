#include "config.hpp"

#include "arg_parser.hpp"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

namespace config {

namespace {

/// Defaults are not repeated here. A field absent from the file contributes no
/// argument, so arg_parser's own default stands -- one place to change it.
const std::vector<Field> kFields = {
    // --- Rider -------------------------------------------------------------
    {"ftp",        "Functional threshold power", "W",     Kind::Real, "Rider",
     "Threshold power; the reference for IF, TSS and the power zones."},
    {"weight",     "Body weight",                "kg",    Kind::Real, "Rider",
     "Used for W/kg, and added to the bike unless a total mass is set."},
    {"bike",       "Bike weight",                "kg",    Kind::Real, "Rider",
     "Added to body weight to give the mass the physics model accelerates."},
    {"lthr",       "Lactate-threshold HR",       "bpm",   Kind::Real, "Rider",
     "Enables heart-rate zones; 0 leaves them off."},
    {"max-hr",     "Maximum HR",                 "bpm",   Kind::Real, "Rider",
     "Fallback reference for HR zones when no LTHR is given."},
    {"crank",      "Crank length",               "mm",    Kind::Real, "Rider",
     "Crank-arm length, for the pedal force in the quadrant analysis."},

    // --- Physics -----------------------------------------------------------
    {"crr",        "Rolling resistance",         "",      Kind::Real, "Physics",
     "Coefficient for the tyres and surface being ridden."},
    {"cda",        "Drag area CdA",              "m^2",   Kind::Real, "Physics",
     "Frontal area times drag coefficient, for the aerodynamic term."},
    {"drivetrain", "Drivetrain efficiency",      "",      Kind::Real, "Physics",
     "Fraction of pedal power reaching the road; 0 to 1."},
    {"smooth",     "Speed smoothing window",     "s",     Kind::Real, "Physics",
     "Tames GPS speed spikes before the power estimate; 0 turns it off."},
    {"max-accel",  "Acceleration clamp",         "m/s^2", Kind::Real, "Physics",
     "Bound on the inertia term, so a GPS jump is not read as a sprint."},
    {"max-speed",  "Speed cap",                  "m/s",   Kind::Real, "Physics",
     "Steps faster than this are dropped as position errors."},
    {"max-grade",  "Gradient clamp",             "",      Kind::Real, "Physics",
     "Bound on gradient as a fraction, against elevation drift."},
    {"max-gap",    "Stop threshold",             "s",     Kind::Real, "Physics",
     "Steps longer than this count as a stop rather than as riding."},

    // --- Analysis ----------------------------------------------------------
    {"points",     "Track points listed",        "",      Kind::Int,  "Analysis",
     "How many points the report prints first; 0 suppresses the listing."},
    {"splits",     "Split length",               "km",    Kind::Real, "Analysis",
     "Length of each split in the per-distance table; 0 turns it off."},
    {"hist-bin",   "Power histogram bin",        "W",     Kind::Real, "Analysis",
     "Width of each band in the power histogram."},
    {"acf-dt",     "Spectral resample interval", "s",     Kind::Real, "Analysis",
     "Uniform grid for the transforms; 0 picks the median sample interval."},
    {"wind",       "Fetch historical wind",      "",      Kind::Flag, "Analysis",
     "Apply Open-Meteo wind to the aero term. The only setting here that "
     "reaches the network."},
};

const Field* find(const std::string& key) {
    for (const Field& f : kFields)
        if (f.key == key) return &f;
    return nullptr;
}

/// Create every directory along `path`, like `mkdir -p`.
Bool make_directories(const std::string& path) {
    for (std::string::size_type i = 1; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != '/') continue;
        const std::string part = path.substr(0, i);
        if (::mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
    }
    return true;
}

std::string directory_of(const std::string& path) {
    const std::string::size_type slash = path.find_last_of('/');
    return (slash == std::string::npos) ? std::string() : path.substr(0, slash);
}

/// Read and parse; `missing` distinguishes "no file" from "bad file".
Bool read_json(const std::string& path, nlohmann::json& out, Bool& missing,
               std::string& err) {
    missing = false;
    std::ifstream in(path);
    if (!in) { missing = true; return false; }
    try {
        in >> out;
    } catch (const std::exception& e) {
        err = "config " + path + ": " + e.what();
        return false;
    }
    if (!out.is_object()) {
        err = "config " + path + ": expected a JSON object at the top level";
        return false;
    }
    return true;
}

/// A JSON scalar as the text a command line would carry.
std::string as_text(const nlohmann::json& v) {
    if (v.is_string())          return v.get<std::string>();
    if (v.is_boolean())         return v.get<bool>() ? "true" : "false";
    if (v.is_number_integer())  return std::to_string(v.get<long long>());
    if (v.is_number()) {
        // Not std::to_string: it pads to six decimals, so 0.3 comes back as
        // "0.300000" and every value in the editor looks like a readout.
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.10g", v.get<double>());
        return buf;
    }
    return {};
}

} // namespace

const std::vector<Field>& fields() { return kFields; }

std::map<std::string, std::string> builtin_defaults() {
    // Run the parser on a bare command line rather than restating its defaults:
    // several of them (the rider/bike split, in particular) are resolved inside
    // parse() and are not readable from a default-constructed Options.
    std::string        argv0 = "gpxana", dummy = "ride.gpx";
    std::vector<char*> raw{argv0.data(), dummy.data()};

    arg_parser::Options o;
    std::string         ignored;
    arg_parser::parse(static_cast<Int>(raw.size()), raw.data(), o, ignored);

    auto num = [](Real v) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.10g", v);   // 0.32, not 0.320000
        return std::string(buf);
    };

    return {
        {"ftp",        num(o.ftp_w)},
        {"weight",     num(o.body_mass_kg)},
        {"bike",       num(o.power.total_mass_kg - o.body_mass_kg)},
        {"lthr",       num(o.lthr)},
        {"max-hr",     num(o.max_hr)},
        {"crank",      num(o.crank_length_m * 1000.0)},
        {"crr",        num(o.power.crr)},
        {"cda",        num(o.power.cda)},
        {"drivetrain", num(o.power.drivetrain_eff)},
        {"smooth",     num(o.power.smooth_window_s)},
        {"max-accel",  num(o.power.max_accel_ms2)},
        {"max-speed",  num(o.power.max_speed_ms)},
        {"max-grade",  num(o.power.max_grade)},
        {"max-gap",    num(static_cast<Real>(o.power.max_gap_s))},
        {"points",     std::to_string(o.max_print)},
        {"splits",     num(o.split_km)},
        {"hist-bin",   num(o.hist_bin_w)},
        {"acf-dt",     num(o.acf_dt)},
        {"wind",       "false"},
    };
}

std::string default_path() {
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"))
        if (xdg[0] != '\0') return std::string(xdg) + "/gpxana/config.json";
    if (const char* home = std::getenv("HOME"))
        if (home[0] != '\0') return std::string(home) + "/.config/gpxana/config.json";
    return {};
}

std::vector<std::string> load_args(const std::string& path, std::string& err) {
    err.clear();
    std::vector<std::string> args;
    if (path.empty()) return args;

    nlohmann::json j;
    Bool missing = false;
    if (!read_json(path, j, missing, err)) return args;   // no file: defaults stand

    std::string unknown;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const Field* f = find(it.key());
        if (!f) { unknown += (unknown.empty() ? "" : ", ") + it.key(); continue; }

        if (f->kind == Kind::Flag) {
            // A flag carries no value, so only its presence is expressed; a
            // false entry contributes nothing rather than an empty argument.
            const Bool on = it.value().is_boolean() ? it.value().get<bool>()
                                                    : (as_text(it.value()) == "true");
            if (on) args.push_back("--" + f->key);
            continue;
        }
        const std::string text = as_text(it.value());
        if (text.empty()) continue;
        args.push_back("--" + f->key);
        args.push_back(text);
    }
    if (!unknown.empty())
        err = "config " + path + ": ignoring unknown setting(s): " + unknown;
    return args;
}

std::map<std::string, std::string> load_values(const std::string& path,
                                               std::string& err) {
    err.clear();
    std::map<std::string, std::string> values;
    if (path.empty()) return values;

    nlohmann::json j;
    Bool missing = false;
    if (!read_json(path, j, missing, err)) return values;

    for (auto it = j.begin(); it != j.end(); ++it)
        if (find(it.key())) values[it.key()] = as_text(it.value());
    return values;
}

Bool save(const std::string& path,
          const std::map<std::string, std::string>& values, std::string& err) {
    err.clear();
    if (path.empty()) { err = "no config path available"; return false; }

    const std::string dir = directory_of(path);
    if (!dir.empty() && !make_directories(dir)) {
        err = "could not create " + dir;
        return false;
    }

    // Written in the order the fields are declared, so the file reads like the
    // dialog rather than in whatever order a map happened to hold it.
    nlohmann::ordered_json j;
    for (const Field& f : kFields) {
        const auto it = values.find(f.key);
        if (it == values.end() || it->second.empty()) continue;

        if (f.kind == Kind::Flag)      j[f.key] = (it->second == "true");
        else if (f.kind == Kind::Int)  j[f.key] = std::atoll(it->second.c_str());
        else                           j[f.key] = std::atof(it->second.c_str());
    }

    std::ofstream out(path);
    if (!out) { err = "could not write " + path; return false; }
    out << j.dump(2) << "\n";
    if (!out) { err = "could not write " + path; return false; }
    return true;
}

} // namespace config
