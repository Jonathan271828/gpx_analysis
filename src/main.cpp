#include "app.hpp"
#include "arg_parser.hpp"
#include "types.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

Int main(Int argc, Char* argv[]) {
    // Parse the command line (arg_parser), then hand off to the app driver.
    arg_parser::Options opts;
    std::string parse_error;
    if (!arg_parser::parse(argc, argv, opts, parse_error)) {
        if (!parse_error.empty()) std::cerr << parse_error << "\n";
        arg_parser::print_usage(argv[0], std::cerr);
        return EXIT_FAILURE;
    }
    return app::run(opts);
}
