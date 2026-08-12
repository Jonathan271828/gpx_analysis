#pragma once

/**
 * @file trends.hpp
 * @brief Multi-ride training trend: turns a set of dated rides (each with a
 *        Training Stress Score) into the fitness / fatigue / form curves.
 *
 * CTL (Chronic Training Load) is a 42-day exponential average of daily TSS
 * (fitness); ATL (Acute Training Load) is the 7-day average (fatigue); TSB
 * (Training Stress Balance) is CTL - ATL (form).
 */

#include "types.hpp"

#include <string>
#include <vector>

namespace trends {

/** @brief One ride's contribution to the trend. */
struct RideLoad {
    std::string date;       /**< Ride date, "YYYY-MM-DD". */
    std::string label;      /**< Ride/file name (for display). */
    Real        tss = 0.0;  /**< Training Stress Score for the ride. */
};

/** @brief The trend as of one ride day. */
struct TrendPoint {
    std::string date;       /**< The ride day, "YYYY-MM-DD". */
    std::string label;      /**< Ride/file name. */
    Real        tss = 0.0;  /**< That day's total TSS. */
    Real        ctl = 0.0;  /**< Chronic Training Load (fitness). */
    Real        atl = 0.0;  /**< Acute Training Load (fatigue). */
    Real        tsb = 0.0;  /**< Training Stress Balance (form), entering the day. */
};

/**
 * @brief Compute CTL/ATL/TSB day by day across the span of the rides.
 * @param rides The rides (any order; multiple per day are summed).
 * @return One row per ride day, in date order. Empty on bad input (e.g. no
 *         parseable dates).
 */
std::vector<TrendPoint> progression(std::vector<RideLoad> rides);

} // namespace trends
