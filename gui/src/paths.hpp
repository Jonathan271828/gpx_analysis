#pragma once

/// @file paths.hpp
/// @brief Splitting a file path, in the one place that does it.
///
/// Four call sites had each written `path.find_last_of('/')` and handled
/// `npos` themselves, which is four chances to get the off-by-one on the
/// separator wrong.

#include <string>

namespace gui::paths {

/// @brief The file name alone.
/// @param path Any path.
/// @return Everything after the last `/`, or @p path when it has none.
std::string basename_of(const std::string& path);

/// @brief The directory a path sits in, with its trailing separator.
/// @param path Any path.
/// @return Everything up to and including the last `/`, so appending a file
///         name yields a sibling path; empty when @p path has no directory
///         part, which appends correctly as a relative name.
std::string directory_of(const std::string& path);

} // namespace gui::paths
