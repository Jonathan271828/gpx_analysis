#include "spectral_view.hpp"

#include "palette.hpp"   // series_colour

#include "imgui.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace gui {

namespace {

const ImVec4 kTextSecondary = ImVec4(0.765f, 0.761f, 0.718f, 1.00f);  // #c3c2b7
const ImVec4 kSurface       = ImVec4(0.102f, 0.102f, 0.098f, 1.00f);  // #1a1a19
const ImVec4 kGrid          = ImVec4(1.000f, 1.000f, 1.000f, 0.09f);  // recessive

// Default lag window. The autocorrelation of a whole ride runs to thousands of
// seconds, where it is only noise; the structure worth seeing (pedalling,
// heart-rate drift, terrain repeats) sits inside the first few minutes.
constexpr double kDefaultMaxLagS = 600.0;

void push_plot_style() {
    ImPlot::PushStyleColor(ImPlotCol_FrameBg,  kSurface);
    ImPlot::PushStyleColor(ImPlotCol_PlotBg,   kSurface);
    ImPlot::PushStyleColor(ImPlotCol_AxisGrid, kGrid);
}

// Number of leading entries of lag_s/acf that mean anything. The four output
// vectors are padded to a common length with NaN, and feeding those to ImPlot
// breaks the line.
int acf_count(const signal::SpectralResult& r) {
    return static_cast<int>(std::min(r.acf.size(), r.n_samples));
}

// A lag in seconds, with the minutes reading alongside once it passes a minute.
// Ride structure repeats on the scale of minutes, and "1830 s" is harder to
// place than "30:30".
std::string format_lag(double s) {
    char buf[64];
    if (s < 60.0) {
        std::snprintf(buf, sizeof buf, "%.1f s", s);
        return buf;
    }
    const long t = static_cast<long>(s + 0.5);
    std::snprintf(buf, sizeof buf, "%.0f s  (%ld:%02ld)", s, t / 60, t % 60);
    return buf;
}

// Where the cursor is, in the plot's own coordinates. Both spectral plots carry
// ImPlotFlags_NoMouseText, so this replaces ImPlot's unlabelled corner readout
// with a tooltip that names the axes and carries their units.
//
// The position is the cursor's, not the nearest sample's: at these densities --
// 8193 bins across the frequency axis, thousands of lags -- snapping would move
// the readout by less than a pixel while hiding where the pointer actually is.
void position_tooltip(const char* x_line, const char* y_line) {
    if (!ImGui::BeginTooltip()) return;
    ImGui::TextUnformatted(x_line);
    ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
    ImGui::TextUnformatted(y_line);
    ImGui::PopStyleColor();
    ImGui::EndTooltip();
}

// The PSD of a channel measured in `unit` is in unit^2/Hz. A compound unit needs
// brackets to say so: "km/h^2/Hz" reads as hours squared.
std::string unit_squared(const std::string& unit) {
    const bool compound = unit.find_first_of("/ ^") != std::string::npos;
    return compound ? "(" + unit + ")^2/Hz" : unit + "^2/Hz";
}

} // namespace

// Header line 1 is "# GPXAna autocorrelation & power spectrum: <name> (<unit>)".
// Recovering the two names keeps a loaded file labelled and its axis in the
// right units; anything unrecognised falls back to the file's own basename.
static void parse_header(const std::string& line, Spectrum& out) {
    const std::string::size_type colon = line.rfind(": ");
    if (colon == std::string::npos) return;
    std::string rest = line.substr(colon + 2);

    const std::string::size_type open = rest.rfind(" (");
    if (open != std::string::npos && rest.back() == ')') {
        out.unit = rest.substr(open + 2, rest.size() - open - 3);
        rest.erase(open);
    }
    if (!rest.empty()) out.name = rest;
}

bool load_spectrum_file(const std::string& path, Spectrum& out, std::string& err) {
    std::ifstream in(path);
    if (!in) { err = "could not open " + path; return false; }

    const std::string::size_type slash = path.find_last_of('/');
    out.name = (slash == std::string::npos) ? path : path.substr(slash + 1);
    out.unit.clear();
    out.result = signal::SpectralResult{};

    signal::SpectralResult& r = out.result;
    std::string line;
    bool        first_comment = true;
    Size        n_lag         = 0;   // rows before the NaN padding starts

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') {
            if (first_comment) { parse_header(line, out); first_comment = false; }
            continue;
        }
        // sscanf parses "nan"; operator>> is not reliable for it, and silently
        // dropping those rows would cut the spectrum short of Nyquist.
        double lag = 0.0, acf = 0.0, f = 0.0, p = 0.0;
        if (std::sscanf(line.c_str(), "%lf %lf %lf %lf", &lag, &acf, &f, &p) != 4)
            continue;
        r.lag_s.push_back(lag);
        r.acf.push_back(acf);
        r.freq_hz.push_back(f);
        r.psd.push_back(p);
        if (!std::isnan(acf)) n_lag = r.acf.size();
    }

    if (r.freq_hz.size() < 4) {
        err = "no usable 4-column rows in " + path;
        return false;
    }

    // n_samples is what the ACF plot trims to; the header carries M but deriving
    // it from the data keeps a hand-edited file consistent.
    r.n_samples = n_lag;
    r.dt_s      = (r.lag_s.size() > 1) ? r.lag_s[1] - r.lag_s[0] : 1.0;
    if (!(r.dt_s > 0.0)) r.dt_s = 1.0;
    r.valid     = true;
    if (out.unit.empty()) out.unit = "unit";
    err.clear();
    return true;
}

void draw_acf_plot(const std::vector<Spectrum>& spectra, bool full_lag,
                   float width, float height) {
    if (spectra.empty()) return;

    double max_lag = 0.0;
    for (const Spectrum& s : spectra) {
        const int n = acf_count(s.result);
        if (n > 1) max_lag = std::max(max_lag, s.result.lag_s[static_cast<std::size_t>(n) - 1]);
    }
    if (max_lag <= 0.0) return;

    const double x_hi = full_lag ? max_lag : std::min(max_lag, kDefaultMaxLagS);

    push_plot_style();
    // The id carries the lag range, so switching it re-fits instead of keeping
    // the other range's limits.
    if (ImPlot::BeginPlot(full_lag ? "##acf_full" : "##acf_short",
                          ImVec2(width, height),
                          ImPlotFlags_NoTitle | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("lag (s)", "autocorrelation");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, x_hi, ImPlotCond_Once);
        ImPlot::SetupAxisLimits(ImAxis_Y1, -1.05, 1.05, ImPlotCond_Once);

        // Zero crossing: where a signal stops resembling itself.
        const double zero = 0.0;
        ImPlotSpec   ref;
        ref.LineColor  = ImVec4(1.0f, 1.0f, 1.0f, 0.25f);
        ref.LineWeight = 1.0f;
        ref.Flags      = ImPlotInfLinesFlags_Horizontal;
        ImPlot::PlotInfLines("##zero", &zero, 1, ref);

        for (std::size_t i = 0; i < spectra.size(); ++i) {
            const signal::SpectralResult& r = spectra[i].result;
            const int n = acf_count(r);
            if (n < 2) continue;

            ImPlotSpec line;
            line.LineColor  = series_colour(i);
            line.LineWeight = 2.0f;
            ImPlot::PlotLine(spectra[i].name.c_str(), r.lag_s.data(), r.acf.data(),
                             n, line);
        }

        if (ImPlot::IsPlotHovered()) {
            const ImPlotPoint m = ImPlot::GetPlotMousePos();
            char xl[64], yl[64];
            std::snprintf(xl, sizeof xl, "lag  %s", format_lag(m.x).c_str());
            std::snprintf(yl, sizeof yl, "autocorrelation  %+.4f", m.y);
            position_tooltip(xl, yl);
        }
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleColor(3);
}

void draw_psd_plots(const std::vector<Spectrum>& spectra, bool refit,
                    float width, float height) {
    if (spectra.empty()) return;

    for (std::size_t i = 0; i < spectra.size(); ++i) {
        const Spectrum&               s = spectra[i];
        const signal::SpectralResult& r = s.result;

        const std::string unit2 = unit_squared(s.unit);

        ImGui::PushStyleColor(ImGuiCol_Text, kTextSecondary);
        ImGui::Text("%s  -  %s,  resampled at %.3g s,  %zu samples",
                    s.name.c_str(), unit2.c_str(), r.dt_s, r.n_samples);
        ImGui::PopStyleColor();

        // Every row of the file, the DC bin included: this is columns 3 and 4 as
        // written, the same set of points `plot 'x.acf.dat' u 3:4 w l` draws.
        const int n = static_cast<int>(std::min(r.freq_hz.size(), r.psd.size()));
        if (n < 3) { ImGui::TextDisabled("  (too few frequency bins)"); continue; }

        // Keyed by channel, not by position: ImPlot stores a plot's axis range
        // under its id, so reusing "##psd0" for whichever channel is first in
        // the list makes power inherit velocity's range.
        const std::string id = "##psd_" + s.name;

        // Linear axes from the origin, matching gnuplot's autoscale on this
        // data. The range is set here rather than left to ImPlot's auto-fit,
        // which stores a plot's range under its id and only ever fits the first
        // frame that id is seen -- so a recomputed channel would otherwise keep
        // the previous one's axes.
        double x_hi = 0.0, y_hi = 0.0;
        for (int k = 0; k < n; ++k) {
            if (std::isfinite(r.freq_hz[k])) x_hi = std::max(x_hi, r.freq_hz[k]);
            if (std::isfinite(r.psd[k]))     y_hi = std::max(y_hi, r.psd[k]);
        }
        if (!(x_hi > 0.0)) x_hi = 1.0;
        if (!(y_hi > 0.0)) y_hi = 1.0;
        const ImPlotCond cond = refit ? ImPlotCond_Always : ImPlotCond_Once;

        push_plot_style();
        if (ImPlot::BeginPlot(id.c_str(), ImVec2(width, height),
                              ImPlotFlags_NoTitle | ImPlotFlags_NoLegend |
                              ImPlotFlags_NoMouseText)) {
            ImPlot::SetupAxes("frequency (Hz)", unit2.c_str());
            ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, x_hi,          cond);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, y_hi * 1.05,   cond);

            ImPlotSpec line;
            line.LineColor  = series_colour(i);
            line.LineWeight = 2.0f;
            ImPlot::PlotLine(s.name.c_str(), r.freq_hz.data(), r.psd.data(),
                             n, line);

            if (ImPlot::IsPlotHovered()) {
                const ImPlotPoint m = ImPlot::GetPlotMousePos();
                char xl[96], yl[96];
                // The period is the same reading as the frequency, in the units
                // the autocorrelation tab uses, so a feature can be matched
                // between the two tabs without arithmetic in your head.
                if (m.x > 0.0)
                    std::snprintf(xl, sizeof xl, "%.6g Hz   (period %s)",
                                  m.x, format_lag(1.0 / m.x).c_str());
                else
                    std::snprintf(xl, sizeof xl, "%.6g Hz", m.x);
                std::snprintf(yl, sizeof yl, "%.6g %s", m.y, unit2.c_str());
                position_tooltip(xl, yl);
            }
            ImPlot::EndPlot();
        }
        ImPlot::PopStyleColor(3);

        ImGui::Dummy(ImVec2(0.0f, 6.0f));
    }
}

} // namespace gui
