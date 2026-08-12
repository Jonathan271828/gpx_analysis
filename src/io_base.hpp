#pragma once

#include "types.hpp"

#include <string>

// ---------------------------------------------------------------------------
// Shared I/O helpers
//
// Small formatting utilities common to the screen-output and file-output
// modules (and to main). Anything used by more than one of those lives here.
// ---------------------------------------------------------------------------

namespace io {

/// Format a duration in seconds as "Xh Ym Zs" (leading units omitted when zero).
std::string format_duration(Long seconds);

} // namespace io
