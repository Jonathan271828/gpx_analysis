#pragma once

/**
 * @file io_base.hpp
 * @brief Shared I/O helpers common to the screen-output and file-output modules.
 */

#include "types.hpp"

#include <string>

namespace io {

/**
 * @brief Format a duration as "Xh Ym Zs" (leading units omitted when zero).
 * @param seconds Duration in seconds.
 * @return The formatted string, e.g. "1h 05m 30s" or "45s".
 */
std::string format_duration(Long seconds);

} // namespace io
