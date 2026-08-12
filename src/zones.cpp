#include "zones.hpp"

#include <array>
#include <string>

namespace zones {

namespace {

constexpr Long STOP_S = 20;   // steps longer than this are stops, not riding

/// Add `seconds` to the first zone whose [lo, hi) contains `value` (the last
/// zone is open-topped).
void bucket(std::vector<Zone>& zs, Real value, Real seconds) {
    for (Size k = 0; k < zs.size(); ++k) {
        const Bool top = (zs[k].hi < 0.0);
        if (value >= zs[k].lo && (top || value < zs[k].hi)) {
            zs[k].seconds += seconds;
            return;
        }
    }
}

std::string watts(Real w) { return std::to_string(static_cast<int>(w + 0.5)); }

} // namespace

// ---------------------------------------------------------------------------
// power_zones — Coggan 7 zones as fractions of FTP
// ---------------------------------------------------------------------------

ZoneTable power_zones(const Track& track, const PowerAnalysis& pa, Real ftp_w) {
    ZoneTable z;
    z.kind = "power";
    if (!pa.stats.valid || ftp_w <= 0.0) return z;

    const auto& pts = track.points;
    const Size  n   = pts.size();
    if (pa.point_power_w.size() != n || pa.t_offset_s.size() != n) return z;

    struct Def { const char* label; Real lo; Real hi; };  // fractions of FTP
    const std::array<Def, 7> defs = {{
        {"Z1 Active recovery", 0.00, 0.55},
        {"Z2 Endurance",       0.55, 0.75},
        {"Z3 Tempo",           0.75, 0.90},
        {"Z4 Threshold",       0.90, 1.05},
        {"Z5 VO2max",          1.05, 1.20},
        {"Z6 Anaerobic",       1.20, 1.50},
        {"Z7 Neuromuscular",   1.50, -1.0},
    }};
    for (const Def& d : defs)
        z.zones.push_back({d.label, d.lo * ftp_w,
                           d.hi < 0.0 ? -1.0 : d.hi * ftp_w, 0.0});

    for (Size i = 1; i < n; ++i) {
        if (pa.t_offset_s[i] < 0 || pa.t_offset_s[i - 1] < 0) continue;
        const Long dt = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
        if (dt <= 0 || dt > STOP_S) continue;
        bucket(z.zones, pa.point_power_w[i], static_cast<Real>(dt));
        z.total_s += static_cast<Real>(dt);
    }

    z.basis = "FTP " + watts(ftp_w) + " W";
    z.valid = z.total_s > 0.0;
    return z;
}

// ---------------------------------------------------------------------------
// hr_zones — 5 zones as fractions of LTHR (preferred) or max HR
// ---------------------------------------------------------------------------

ZoneTable hr_zones(const Track& track, const PowerAnalysis& pa,
                   Real lthr, Real max_hr) {
    ZoneTable z;
    z.kind = "heart rate";

    const auto& pts = track.points;
    const Size  n   = pts.size();
    if (pa.t_offset_s.size() != n) return z;

    struct Def { const char* label; Real lo; Real hi; };
    std::array<Def, 5> defs{};
    Real ref = 0.0;
    if (lthr > 0.0) {
        ref = lthr;
        defs = {{ {"Z1 Recovery",   0.00, 0.68},
                  {"Z2 Endurance",  0.68, 0.83},
                  {"Z3 Tempo",      0.83, 0.94},
                  {"Z4 Threshold",  0.94, 1.05},
                  {"Z5 VO2max",     1.05, -1.0} }};
        z.basis = "LTHR " + std::to_string(static_cast<int>(lthr + 0.5)) + " bpm";
    } else if (max_hr > 0.0) {
        ref = max_hr;
        defs = {{ {"Z1 Recovery",   0.00, 0.60},
                  {"Z2 Endurance",  0.60, 0.70},
                  {"Z3 Tempo",      0.70, 0.80},
                  {"Z4 Threshold",  0.80, 0.90},
                  {"Z5 VO2max",     0.90, -1.0} }};
        z.basis = "max HR " + std::to_string(static_cast<int>(max_hr + 0.5)) + " bpm";
    } else {
        return z;   // no reference given
    }

    for (const Def& d : defs)
        z.zones.push_back({d.label, d.lo * ref, d.hi < 0.0 ? -1.0 : d.hi * ref, 0.0});

    for (Size i = 1; i < n; ++i) {
        if (pa.t_offset_s[i] < 0 || pa.t_offset_s[i - 1] < 0) continue;
        const Long dt = pa.t_offset_s[i] - pa.t_offset_s[i - 1];
        if (dt <= 0 || dt > STOP_S) continue;
        if (!pts[i].has_hr) continue;
        bucket(z.zones, static_cast<Real>(pts[i].hr), static_cast<Real>(dt));
        z.total_s += static_cast<Real>(dt);
    }

    z.valid = z.total_s > 0.0;
    return z;
}

} // namespace zones
