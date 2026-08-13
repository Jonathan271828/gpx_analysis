#pragma once

/**
 * @file config.hpp
 * @brief Persistent defaults, shared by both front-ends.
 *
 * The rider's own numbers -- threshold power, weight, the bike's aerodynamics --
 * do not change from run to run, and retyping them as flags every time is how
 * they end up wrong. They live in one JSON file instead.
 *
 * The file does not introduce a second way of deciding what an option means. It
 * is read into command-line arguments and handed to arg_parser like any other,
 * ahead of the real ones, so a flag typed on the command line still wins and
 * arg_parser remains the only place an option is interpreted. A setting added
 * to the parser becomes configurable without touching this file.
 *
 * Nothing is written unless asked. With no config file the built-in defaults
 * stand exactly as before.
 */

#include "types.hpp"

#include <map>
#include <string>
#include <vector>

namespace config {

/** @brief How a setting is entered and presented. */
enum class Kind {
    Real,  /**< A decimal number. */
    Int,   /**< A whole number. */
    Flag   /**< Present or absent; carries no value. */
};

/** @brief One configurable setting, and the option it maps to. */
struct Field {
    std::string key;      /**< JSON key, and the CLI flag without its dashes. */
    std::string label;    /**< Human-readable name. */
    std::string unit;     /**< Unit for display; empty when dimensionless. */
    Kind        kind = Kind::Real;
    std::string group;    /**< "Rider", "Physics" or "Analysis". */
    std::string help;     /**< One line, matching the flag's usage text. */
};

/**
 * @brief Every setting that may appear in the file.
 *
 * Deliberately not all of arg_parser's options: export paths and segment
 * queries describe one run rather than the rider, and a config that carried
 * them would write files on every invocation.
 *
 * @return The fields, in presentation order.
 */
const std::vector<Field>& fields();

/**
 * @brief The values in force with no config file and no flags.
 *
 * Obtained by running the parser rather than by restating its defaults here,
 * so an editor shows what the tool would actually do and cannot drift from it.
 *
 * @return Field key to value as text.
 */
std::map<std::string, std::string> builtin_defaults();

/**
 * @brief Where the config lives.
 * @return `$XDG_CONFIG_HOME/gpxana/config.json`, or `~/.config/gpxana/...`
 *         when that variable is unset; empty if neither can be determined.
 */
std::string default_path();

/**
 * @brief Read the file as command-line arguments.
 * @param path Config path; a missing file is not an error.
 * @param err  Out: set only when the file exists but could not be used.
 * @return Arguments such as `{"--ftp", "290"}`, empty when there is no file.
 *         Keys that match no field are skipped and reported through @p err,
 *         so a typo is visible rather than silently ignored.
 */
std::vector<std::string> load_args(const std::string& path, std::string& err);

/**
 * @brief Read the file as raw values, for an editor.
 * @param path Config path; a missing file yields an empty map.
 * @param err  Out: set only when the file exists but could not be read.
 * @return Field key to value as text; flags read as "true"/"false".
 */
std::map<std::string, std::string> load_values(const std::string& path,
                                               std::string& err);

/**
 * @brief Write the values, creating the directory if needed.
 * @param path   Config path.
 * @param values Field key to value as text; unknown keys are dropped.
 * @param err    Out: reason for failure.
 * @return True on success.
 */
Bool save(const std::string& path,
          const std::map<std::string, std::string>& values, std::string& err);

} // namespace config
