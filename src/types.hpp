#pragma once

/**
 * @file types.hpp
 * @brief Project-wide scalar type aliases used throughout the codebase.
 *
 * A single vocabulary for the fundamental scalar types. Standard-library class
 * types (std::string, std::vector, ...) are deliberately left un-aliased.
 */

#include <cstddef>
#include <ctime>

using Real = double;         /**< Floating-point quantities (metres, watts, ...). */
using Int  = int;            /**< Small signed integers / sensor readings. */
using Long = long;           /**< Wide signed integers (durations in seconds, ...). */
using Size = std::size_t;    /**< Sizes, counts and container indices. */
using Bool = bool;           /**< Boolean flags. */
using Char = char;           /**< Characters / raw byte buffers. */
using Time = std::time_t;    /**< Absolute timestamps (UTC seconds since epoch). */
