#pragma once

#include <cstddef>
#include <ctime>

// ---------------------------------------------------------------------------
// Project-wide scalar type aliases
//
// A single vocabulary for the fundamental scalar types used throughout the
// codebase. Standard library class types (std::string, std::vector, ...) are
// deliberately left un-aliased.
// ---------------------------------------------------------------------------

using Real = double;         // floating-point quantities (metres, watts, ...)
using Int  = int;            // small signed integers / sensor readings
using Long = long;           // wide signed integers (durations in seconds, ...)
using Size = std::size_t;    // sizes, counts and container indices
using Bool = bool;           // boolean flags
using Char = char;           // characters / raw byte buffers
using Time = std::time_t;    // absolute timestamps (UTC seconds since epoch)
