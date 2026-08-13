#include "profile.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace profile {

namespace {

/// The four durations that span the curve, with the ratio to threshold power a
/// balanced rider tends to show at each.
///
/// These are anchors, not percentiles. They are printed alongside the rider's
/// own ratios precisely so the comparison can be judged rather than taken on
/// trust: what the profile actually reports is the *departure* from a flat
/// shape, which is robust even if an anchor is a little off, because every
/// duration is displaced by the same error.
struct Anchor {
    Long        duration_s;
    const char* label;
    Real        ratio;
};

constexpr std::array<Anchor, 4> kAnchors{{
    {   5, "5 s",    4.50},   // neuromuscular
    {  60, "1 min",  1.90},   // anaerobic
    { 300, "5 min",  1.20},   // aerobic power
    {1200, "20 min", 1.05},   // threshold
}};

/// How far a relative score must sit from 1 before it is called a departure
/// rather than noise. A ride is one sample of a rider, so the bar is not low.
constexpr Real kNotable = 0.15;

const Point* at(const std::vector<Point>& pts, Long duration_s) {
    for (const Point& p : pts)
        if (p.duration_s == duration_s) return p.found ? &p : nullptr;
    return nullptr;
}

/// Whether a short effort is credible as near-maximal.
///
/// Nobody sprints on every ride. If the five-second power barely exceeds the
/// twenty-minute power, the rider did not sprint, and calling them "not a
/// sprinter" on that evidence would be nonsense.
Bool sprinted(const std::vector<Point>& pts) {
    const Point* short_effort = at(pts, 5);
    const Point* long_effort  = at(pts, 1200);
    if (!short_effort || !long_effort || long_effort->watts <= 0.0) return true;
    return short_effort->watts > 2.0 * long_effort->watts;
}

} // namespace

std::string phenotype_name(Phenotype p) {
    switch (p) {
        case Phenotype::Sprinter:     return "Sprinter";
        case Phenotype::Puncheur:     return "Puncheur";
        case Phenotype::AllRounder:   return "All-rounder";
        case Phenotype::Endurance:    return "Endurance rider";
        case Phenotype::Unknown:      break;
    }
    return "Unclear";
}

std::string phenotype_advice(Phenotype p) {
    switch (p) {
        case Phenotype::Sprinter:
            return "Short power is your asset; the gains are in raising the "
                   "threshold underneath it so you arrive at the sprint fresh.";
        case Phenotype::Puncheur:
            return "Built for repeated attacks and short climbs. Work on "
                   "recovery between efforts rather than on peak power.";
        case Phenotype::AllRounder:
            return "No part of the curve stands out. Pick the end that suits "
                   "your racing and develop it deliberately.";
        case Phenotype::Endurance:
            return "Long power is your asset. A little neuromuscular work costs "
                   "almost nothing aerobically and buys a finishing sprint.";
        case Phenotype::Unknown:
            break;
    }
    return {};
}

Profile analyse(const std::vector<peaks::PeakEffort>& pk, Real threshold_w,
                Real body_mass_kg) {
    Profile pr;
    if (pk.empty() || threshold_w <= 0.0) return pr;

    pr.measured     = pk.front().measured;
    pr.threshold_w  = threshold_w;
    pr.body_mass_kg = body_mass_kg;

    for (const Anchor& a : kAnchors) {
        Point p;
        p.duration_s = a.duration_s;
        p.reference  = a.ratio;

        for (const peaks::PeakEffort& e : pk) {
            if (e.duration_s != a.duration_s) continue;
            p.watts    = e.avg_power_w;
            p.wkg      = body_mass_kg > 0.0 ? e.avg_power_w / body_mass_kg : 0.0;
            p.ratio    = e.avg_power_w / threshold_w;
            p.relative = p.ratio / a.ratio;
            p.found    = true;
            break;
        }
        pr.points.push_back(p);
    }

    // Divide out the overall level so only the shape remains: the geometric
    // mean is the right centre here because the scores are ratios, and a
    // threshold set 10 % high divides every one of them by the same factor.
    Real   log_sum = 0.0;
    Size   counted = 0;
    for (const Point& p : pr.points)
        if (p.found && p.relative > 0.0) { log_sum += std::log(p.relative); ++counted; }
    const Real centre = counted ? std::exp(log_sum / static_cast<Real>(counted)) : 1.0;
    for (Point& p : pr.points)
        p.shape = (p.found && centre > 0.0) ? p.relative / centre : 0.0;

    const Size found = static_cast<Size>(
        std::count_if(pr.points.begin(), pr.points.end(),
                      [](const Point& p) { return p.found; }));
    if (found < 3) {
        pr.caveat = "the ride is too short to contain the whole profile";
        return pr;
    }
    pr.valid = true;

    // Strongest and weakest relative to the balanced shape.
    const Point* best  = nullptr;
    const Point* worst = nullptr;
    for (const Point& p : pr.points) {
        if (!p.found) continue;
        if (!best  || p.shape > best->shape)  best  = &p;
        if (!worst || p.shape < worst->shape) worst = &p;
    }
    for (const Anchor& a : kAnchors) {
        if (best  && a.duration_s == best->duration_s)  pr.strength = a.label;
        if (worst && a.duration_s == worst->duration_s) pr.weakness = a.label;
    }

    if (!sprinted(pr.points)) {
        pr.caveat = "no near-maximal short effort in this ride, so the short "
                    "end of the curve reflects the ride rather than the rider";
    } else if (!pr.measured) {
        // The de-spiking that makes an estimate trustworthy over sustained
        // efforts is the same processing that removes a sprint: the speed
        // smoothing averages the spike away and the acceleration clamp bounds
        // precisely what a standing start produces. Estimated profiles
        // therefore lean endurance, and saying so matters more than the label.
        pr.caveat = "estimated power understates short efforts -- speed "
                    "smoothing and the acceleration clamp remove the very "
                    "spike a sprint makes -- so the short end reads low and "
                    "the profile leans toward endurance";
    }

    // Classify from the curve's tilt: the short end weighed against the long
    // one. Because the shape scores are normalised to a geometric mean of 1,
    // any strength implies a matching weakness elsewhere, so an absolute
    // threshold on a single duration would fire almost at random. The ratio
    // between the two ends is the stable quantity.
    auto mean_shape = [&](Long a, Long b) {
        const Point* pa = at(pr.points, a);
        const Point* pb = at(pr.points, b);
        if (pa && pb) return std::sqrt(pa->shape * pb->shape);
        if (pa)       return pa->shape;
        if (pb)       return pb->shape;
        return 1.0;
    };

    const Real short_end = mean_shape(5, 60);
    const Real long_end  = mean_shape(300, 1200);
    const Real tilt      = long_end > 0.0 ? short_end / long_end : 1.0;

    const Point* p5  = at(pr.points, 5);
    const Point* p60 = at(pr.points, 60);

    if (tilt > 1.0 + kNotable) {
        // Both ends of the short range are strong; which one leads says whether
        // it is a standing sprint or a minute-long attack.
        const Real five = p5  ? p5->shape  : 0.0;
        const Real one  = p60 ? p60->shape : 0.0;
        pr.phenotype = (five >= one) ? Phenotype::Sprinter : Phenotype::Puncheur;
    } else if (tilt < 1.0 / (1.0 + kNotable)) {
        pr.phenotype = Phenotype::Endurance;
    } else {
        pr.phenotype = Phenotype::AllRounder;
    }

    // A profile whose short end is not evidence cannot name a sprinter.
    if (!pr.caveat.empty() && pr.phenotype == Phenotype::Sprinter)
        pr.phenotype = Phenotype::AllRounder;

    return pr;
}

} // namespace profile
