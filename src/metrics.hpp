#pragma once

/**
 * @file metrics.hpp
 * @brief Training-analysis metrics: load (NP/IF/TSS/VI, energy, W/kg,
 *        Efficiency Factor) and aerobic decoupling.
 *
 * Everything is derived from the per-step power series in PowerAnalysis (the
 * measured `<power>` when the track carries it, otherwise the estimate).
 */

#include "types.hpp"
#include "gpx_reader.hpp"   // Track, PowerAnalysis

namespace metrics {

/** @brief Training-load summary for one track. */
struct TrainingLoad {
    Bool valid         = false;  /**< True when a usable power series was found. */
    Bool from_measured = false;  /**< True when computed from a real power meter. */
    Real avg_power_w   = 0.0;    /**< Average power over moving time (W). */
    Real np_w          = 0.0;    /**< Normalized Power (W). */
    Real if_           = 0.0;    /**< Intensity Factor = NP / FTP. */
    Real tss           = 0.0;    /**< Training Stress Score. */
    Real vi            = 0.0;    /**< Variability Index = NP / average power. */
    Real ftp_w         = 0.0;    /**< FTP used for IF/TSS (W). */
    Real kj            = 0.0;    /**< Mechanical work (kJ). */
    Real kcal          = 0.0;    /**< Energy (kcal); ~ kj for cycling. */
    Real body_mass_kg  = 0.0;    /**< Rider body mass used for W/kg (kg). */
    Real avg_wkg       = 0.0;    /**< Average power per kilogram (W/kg). */
    Real np_wkg        = 0.0;    /**< Normalized Power per kilogram (W/kg). */
    Long moving_s      = 0;      /**< Moving time used (stops excluded) (s). */
    Bool has_hr        = false;  /**< True if heart rate was available. */
    Real avg_hr        = 0.0;    /**< Mean heart rate over moving time (bpm). */
    Real ef            = 0.0;    /**< Efficiency Factor = NP / avg HR. */
};

/**
 * @brief Compute the training-load summary for a track.
 * @param track        The track to analyse.
 * @param pa           Power analysis (per-step power series + timestamps).
 * @param ftp_w        Functional threshold power (W).
 * @param body_mass_kg Rider body mass for W/kg (kg); pass 0 to skip W/kg.
 * @return Filled TrainingLoad; valid == false when power is unusable.
 */
TrainingLoad training_load(const Track& track, const PowerAnalysis& pa,
                           Real ftp_w, Real body_mass_kg);

/** @brief Aerobic decoupling (Pw:Hr): drift of the power-to-heart-rate ratio. */
struct Decoupling {
    Bool valid        = false; /**< True if heart rate was available in both halves. */
    Real first_ratio  = 0.0;   /**< (avg power / avg HR) over the first moving half. */
    Real second_ratio = 0.0;   /**< ... over the second half. */
    Real pct          = 0.0;   /**< (first - second)/first * 100; + = HR drifted up. */
};

/**
 * @brief Compute aerobic decoupling by comparing the first and second moving
 *        halves of the ride.
 * @param track The track to analyse.
 * @param pa    Power analysis for the same track.
 * @return Filled Decoupling; valid == false without heart-rate data.
 */
Decoupling decoupling(const Track& track, const PowerAnalysis& pa);

} // namespace metrics
