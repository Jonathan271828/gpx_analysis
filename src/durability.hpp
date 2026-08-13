#pragma once

/**
 * @file durability.hpp
 * @brief Fatigue resistance: how the power-duration curve decays as work
 *        accumulates.
 *
 * A peak-power table says what the rider can do; it does not say when in the
 * ride they did it. Twenty minutes at 300 W in the first hour and the same
 * twenty minutes after 2000 kJ are different efforts, and the gap between them
 * is what separates riders who hold their form to the end of a long day from
 * riders who do not. This measures that gap directly: the same best-effort
 * search, restricted to efforts that begin only after a given amount of
 * mechanical work has already been done.
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

#include <vector>

namespace durability {

/** @brief Best average power for one duration at one level of accumulated work. */
struct Effort {
    Long duration_s   = 0;      /**< Window length (s). */
    Real after_kj     = 0.0;    /**< Work already done before the window starts (kJ). */
    Real avg_power_w  = 0.0;    /**< Best average power over such a window (W). */
    Long start_offset_s = 0;    /**< Seconds from the ride start to that window. */
    Bool found        = false;  /**< False when the ride never got that deep. */
};

/** @brief One duration's decay across every work threshold. */
struct Curve {
    Long                duration_s = 0;   /**< Window length (s). */
    std::vector<Effort> efforts;          /**< One per threshold, ascending. */

    /**
     * @brief Drop from the freshest to the deepest effort found, as a percent.
     *
     * Positive means power fell as work accumulated, which is the usual case;
     * negative means the rider's best effort came late, which usually says the
     * early ride was steady rather than that they got stronger.
     */
    Real fade_pct = 0.0;
    Bool valid    = false;      /**< True when at least two thresholds were reached. */
};

/** @brief The whole fatigue-resistance table. */
struct Report {
    Bool              valid    = false;  /**< True when any curve is usable. */
    Bool              measured = false;  /**< True when built from a power meter. */
    Real              total_kj = 0.0;    /**< Work done over the whole ride (kJ). */
    std::vector<Real> thresholds_kj;     /**< The work levels actually reported. */
    std::vector<Curve> curves;           /**< One per requested duration. */
};

/**
 * @brief Build the fatigue-resistance table.
 *
 * For each duration and each work threshold, searches for the best average
 * power over a window that *starts* after that much work has been done. A
 * threshold beyond the ride's total work is dropped rather than reported empty,
 * so a short ride simply yields a short table.
 *
 * @param track         The track to analyse.
 * @param pa            Power analysis (per-step power series and timestamps).
 * @param durations_s   Window lengths to report, ascending.
 * @param thresholds_kj Work levels to report, ascending; 0 means "fresh".
 * @return The report; valid == false when the ride has no usable power.
 */
Report analyse(const Track& track, const PowerAnalysis& pa,
               const std::vector<Long>& durations_s,
               const std::vector<Real>& thresholds_kj);

} // namespace durability
