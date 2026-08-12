#pragma once

#include "types.hpp"
#include "arg_parser.hpp"   // Options

// ---------------------------------------------------------------------------
// Application driver
//
// Ties the modules together: for each input file, parse it, analyse every
// track (report + exports), and — across several files — print the training
// trend. main() only parses the command line and calls run().
// ---------------------------------------------------------------------------

namespace app {

/// Run the whole analysis for the parsed options. Returns the exit status
/// (EXIT_SUCCESS, or EXIT_FAILURE if any file failed to parse).
Int run(const arg_parser::Options& opts);

} // namespace app
