#include "app.hpp"
#include "arg_parser.hpp"
#include "config.hpp"
#include "types.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

/// The command line, with the config file's settings inserted ahead of it.
///
/// Ahead, so that a flag typed by hand still wins: arg_parser assigns as it
/// scans, and the last occurrence of an option is the one that survives.
std::vector<std::string> with_config(Int argc, Char* argv[],
                                     const std::string& path) {
    std::vector<std::string> args;
    args.emplace_back(argv[0]);

    std::string err;
    for (std::string& a : config::load_args(path, err))
        args.push_back(std::move(a));
    if (!err.empty()) std::cerr << err << "\n";

    for (Int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return args;
}

} // namespace

Int main(Int argc, Char* argv[]) {
    // Settings the rider does not retype every run come from the config file,
    // when there is one; without it the built-in defaults stand unchanged.
    const std::string        config_path = config::default_path();
    std::vector<std::string> args        = with_config(argc, argv, config_path);

    std::vector<Char*> raw;
    raw.reserve(args.size());
    for (std::string& a : args) raw.push_back(a.data());

    arg_parser::Options opts;
    std::string         parse_error;
    if (!arg_parser::parse(static_cast<Int>(raw.size()), raw.data(), opts,
                           parse_error) ||
        opts.filepaths.empty()) {
        if (!parse_error.empty()) std::cerr << parse_error << "\n";
        arg_parser::print_usage(argv[0], std::cerr);
        return EXIT_FAILURE;
    }
    return app::run(opts);
}
