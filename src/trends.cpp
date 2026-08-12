#include "trends.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>

namespace trends {

namespace {

/// Parse "YYYY-MM-DD" to a whole-day number (days since the epoch), or -1.
Long day_number(const std::string& date) {
    std::tm tm{};
    if (!strptime(date.c_str(), "%Y-%m-%d", &tm)) return -1;
    tm.tm_isdst = 0;
    const std::time_t t = timegm(&tm);
    if (t == static_cast<std::time_t>(-1)) return -1;
    return static_cast<Long>(t / 86400);
}

struct Day {
    std::string date;
    std::string label;
    int         count = 0;
    Real        tss   = 0.0;
};

} // namespace

std::vector<TrendPoint> progression(std::vector<RideLoad> rides) {
    std::vector<TrendPoint> out;
    if (rides.empty()) return out;

    // Aggregate TSS per calendar day.
    std::map<Long, Day> days;
    Long lo = 0, hi = 0;
    Bool first = true;
    for (const RideLoad& r : rides) {
        const Long d = day_number(r.date);
        if (d < 0) continue;
        Day& day = days[d];
        day.date  = r.date;
        day.count += 1;
        day.tss   += r.tss;
        day.label  = (day.count == 1) ? r.label
                                      : (std::to_string(day.count) + " rides");
        if (first) { lo = hi = d; first = false; }
        else { lo = std::min(lo, d); hi = std::max(hi, d); }
    }
    if (first) return out;   // no parseable dates

    const double k_ctl = 1.0 - std::exp(-1.0 / 42.0);
    const double k_atl = 1.0 - std::exp(-1.0 / 7.0);

    double ctl = 0.0, atl = 0.0;
    for (Long d = lo; d <= hi; ++d) {
        auto it = days.find(d);
        const Real tss = (it != days.end()) ? it->second.tss : 0.0;

        const double tsb = ctl - atl;          // form entering the day
        ctl += (tss - ctl) * k_ctl;
        atl += (tss - atl) * k_atl;

        if (it != days.end()) {
            TrendPoint p;
            p.date  = it->second.date;
            p.label = it->second.label;
            p.tss   = tss;
            p.ctl   = static_cast<Real>(ctl);
            p.atl   = static_cast<Real>(atl);
            p.tsb   = static_cast<Real>(tsb);
            out.push_back(p);
        }
    }
    return out;
}

} // namespace trends
