#include "paths.hpp"

namespace gui::paths {

namespace {
constexpr char kSeparator = '/';
} // namespace

std::string basename_of(const std::string& path) {
    const std::string::size_type slash = path.find_last_of(kSeparator);
    return (slash == std::string::npos) ? path : path.substr(slash + 1);
}

std::string directory_of(const std::string& path) {
    const std::string::size_type slash = path.find_last_of(kSeparator);
    return (slash == std::string::npos) ? std::string() : path.substr(0, slash + 1);
}

} // namespace gui::paths
