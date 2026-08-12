#pragma once

/**
 * @file app.hpp
 * @brief Application driver: ties the modules together. For each input file it
 *        parses, analyses every track (report + exports), and — across several
 *        files — prints the training trend. main() only parses the command line
 *        and calls run().
 */

#include "types.hpp"
#include "arg_parser.hpp"   // Options

namespace app {

/**
 * @brief Run the whole analysis for the parsed options.
 * @param opts The parsed command-line options.
 * @return The process exit status: EXIT_SUCCESS, or EXIT_FAILURE if any file
 *         failed to parse.
 */
Int run(const arg_parser::Options& opts);

} // namespace app
