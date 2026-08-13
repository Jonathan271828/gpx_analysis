#pragma once

/**
 * @file profile.hpp
 * @brief Rider phenotype: the shape of the power-duration curve, read as a
 *        description of what kind of rider produced it.
 *
 * Two riders with the same threshold power can be entirely different athletes.
 * One holds 380 W for five seconds and 300 W for twenty minutes; the other
 * holds 1400 W and 300 W. The second is a sprinter, the first is not, and no
 * single number says so -- the *shape* of the curve does.
 *
 * The shape is measured against the rider's own threshold rather than against a
 * population table. That is deliberate. Published percentile charts are split
 * by sex, age and testing protocol, and applying the wrong one produces a
 * confident answer that is simply wrong; a ratio to your own threshold needs no
 * such assumption and answers the more useful question anyway -- not "how do I
 * compare to strangers" but "which part of my own range is underdeveloped".
 */

#include "types.hpp"
#include "peaks.hpp"   // peaks::PeakEffort

#include <string>
#include <vector>

namespace profile {

/** @brief The kind of rider the curve's shape suggests. */
enum class Phenotype {
    Unknown,       /**< Too few efforts, or none near maximal. */
    Sprinter,      /**< Very short power far above the rest of the curve. */
    Puncheur,      /**< Strong for a minute or two; repeated attacks, short hills. */
    AllRounder,    /**< No part of the curve stands out from the others. */
    Endurance      /**< Long power strong, short power modest; TT and long climbs. */
};

/** @brief One duration of the profile, and how it compares. */
struct Point {
    Long duration_s   = 0;    /**< Effort length (s). */
    Real watts        = 0.0;  /**< Best average power over it (W). */
    Real wkg          = 0.0;  /**< The same per kilogram (W/kg); 0 if no mass. */
    Real ratio        = 0.0;  /**< watts / threshold power. */
    Real reference    = 0.0;  /**< Ratio a balanced rider would show here. */
    Real relative     = 0.0;  /**< ratio / reference; carries the overall level. */
    /**
     * @brief @ref relative with the overall level divided out; 1 is balanced.
     *
     * A threshold setting that is too high drags every duration down by the
     * same factor, which would read as a rider weak at everything rather than
     * as a mis-set number. Dividing by the geometric mean of the four leaves
     * only the shape, which is what a phenotype actually is -- and it is what
     * the classification uses.
     */
    Real shape        = 0.0;
    Bool found        = false;/**< False when the ride had no such effort. */
};

/** @brief The whole profile and what it suggests. */
struct Profile {
    Bool  valid       = false; /**< True when enough of the curve was present. */
    Bool  measured    = false; /**< True when built from a power meter. */
    Real  threshold_w = 0.0;   /**< The reference the ratios are taken against. */
    Real  body_mass_kg = 0.0;  /**< Mass used for W/kg; 0 when unknown. */

    std::vector<Point> points;      /**< One per profile duration, ascending. */
    Phenotype   phenotype = Phenotype::Unknown;
    std::string strength;           /**< Label of the strongest duration. */
    std::string weakness;           /**< Label of the weakest duration. */

    /**
     * @brief Why the profile should not be trusted, when it should not.
     *
     * A ride with no sprint says nothing about sprinting. Rather than reporting
     * a confident phenotype from absent evidence, the reason is stated and the
     * caller can decline to draw a conclusion.
     */
    std::string caveat;

    /**
     * @brief Threshold power this ride's own efforts imply, when that differs
     *        markedly from the configured one; 0 when they agree.
     *
     * Worth surfacing on its own: a profile is read against the threshold, so a
     * threshold that is out of date makes every other number misleading.
     */
    Real suggested_threshold_w = 0.0;
};

/** @brief Human-readable name of a phenotype, e.g. "All-rounder". */
std::string phenotype_name(Phenotype p);

/**
 * @brief One line saying what the phenotype means for training.
 * @param p The phenotype.
 * @return The advice; empty for Phenotype::Unknown.
 */
std::string phenotype_advice(Phenotype p);

/**
 * @brief Build the profile from a ride's peak efforts.
 *
 * Uses the 5 s, 1 min, 5 min and 20 min efforts when present -- the four that
 * span the neuromuscular, anaerobic, aerobic-power and threshold ends of the
 * curve.
 *
 * @param pk           Peak efforts, as peaks::best_efforts returns them.
 * @param threshold_w  Threshold power the ratios are taken against (W).
 * @param body_mass_kg Rider mass for W/kg; pass 0 to omit it.
 * @return The profile; valid == false when too little of the curve is present.
 */
Profile analyse(const std::vector<peaks::PeakEffort>& pk, Real threshold_w,
                Real body_mass_kg);

} // namespace profile
