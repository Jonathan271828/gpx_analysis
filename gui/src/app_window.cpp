#include "app_window.hpp"

#include "file_dialog.hpp"
#include "panel.hpp"
#include "paths.hpp"
#include "file_output.hpp"   // io::write_spectral_file
#include "palette.hpp"
#include "peaks_chart.hpp"
#include "spectral_view.hpp"
#include "theme.hpp"
#include "zone_chart.hpp"

#include "imgui.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace gui {

namespace {

// Offset just past the end of the report section starting with `header`, so a
// chart can be inserted directly below that section's table.
//
// A section runs from its heading to the start of the next one. Headings are the
// only lines beginning with "===" or "---" -- the dashed rule inside a table is
// indented by a space -- so they delimit sections reliably even when a section
// contains blank lines, as the hills table does before its "Total:" line.
//
// Returns npos when the report does not contain the section.
std::size_t section_end(const std::string& report, const std::string& header) {
    const std::size_t start = report.find(header);
    if (start == std::string::npos) return std::string::npos;

    for (std::size_t nl = report.find('\n', start); nl != std::string::npos;) {
        const std::size_t line = nl + 1;
        if (line >= report.size()) return report.size();
        if (report.compare(line, 3, "===") == 0 || report.compare(line, 3, "---") == 0)
            return line;
        nl = report.find('\n', line);
    }
    return report.size();
}

// ImPlot draws y-axis labels outside the plot frame; the panels reserve this
// much of their width so those labels are not clipped by the panel border.
constexpr float kAxisLabelGutter = 10.0f;

/// Which chart a cut in the report text calls for.
enum class ReportChart { Hills, Zones, Peaks };

/// A point in the report text, and the chart that belongs there.
struct ChartCut {
    std::size_t at    = 0;
    ReportChart chart = ReportChart::Hills;
};

/// Locate the end of each illustrated section, in the order they appear.
///
/// Sorting by position rather than trusting the search order means the text is
/// emitted in one pass regardless of how the report happens to be laid out, and
/// a report missing a section simply contributes no cut.
std::vector<ChartCut> find_chart_cuts(const std::string& report) {
    static const std::pair<const char*, ReportChart> kSections[] = {
        {"--- Hills",                ReportChart::Hills},
        {"=== Time in power zones",  ReportChart::Zones},
        {"=== Peak power efforts",   ReportChart::Peaks},
    };

    std::vector<ChartCut> cuts;
    for (const auto& [header, chart] : kSections) {
        const std::size_t at = section_end(report, header);
        if (at != std::string::npos) cuts.push_back({at, chart});
    }
    std::sort(cuts.begin(), cuts.end(),
              [](const ChartCut& a, const ChartCut& b) { return a.at < b.at; });
    return cuts;
}

} // namespace

void AppWindow::load(const std::string& path) {
    if (path.empty()) return;
    path_ = path;

    result_ = analyse(path, static_cast<std::size_t>(max_print_ < 0 ? 0 : max_print_),
                      wind_on_);
    track_  = 0;
    reset_channel_selection();   // transforms belong to the file that produced them
}

void AppWindow::draw() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("GPXAna", nullptr, flags)) {
        draw_toolbar();
        ImGui::Separator();
        draw_banner();

        // The report is one page; the spectral views are a different kind of
        // analysis, computed on demand, so they get tabs of their own.
        if (ImGui::BeginTabBar("views")) {
            if (ImGui::BeginTabItem("Report")) {
                draw_report();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Autocorrelation")) {
                draw_acf_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Power spectrum")) {
                draw_psd_tab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void AppWindow::reset_channel_selection() {
    chan_on_.clear();
    spectra_.clear();
    psd_refit_ = true;
    spectral_note_.clear();
    if (result_.tracks.empty()) return;

    const std::vector<channels::Channel>& ch =
        result_.tracks[static_cast<Size>(track_)].channels;
    // Velocity is on by default: every ride with power has it, and it is the
    // channel whose periodicity is usually asked about first.
    chan_on_.assign(ch.size(), 0);
    for (Size i = 0; i < ch.size(); ++i)
        if (ch[i].name == "velocity") chan_on_[i] = 1;
    if (!ch.empty() && std::none_of(chan_on_.begin(), chan_on_.end(),
                                    [](char c) { return c != 0; }))
        chan_on_[0] = 1;
}

void AppWindow::compute_spectra() {
    spectra_.clear();
    // New data means the PSD plots' stored axis ranges belong to the old data.
    psd_refit_ = true;
    spectral_note_.clear();
    if (result_.tracks.empty()) return;

    const std::vector<channels::Channel>& ch =
        result_.tracks[static_cast<Size>(track_)].channels;

    for (Size i = 0; i < ch.size() && i < chan_on_.size(); ++i) {
        if (!chan_on_[i]) continue;

        signal::SpectralResult r =
            signal::compute_acf_psd(ch[i].t_s, ch[i].value,
                                    static_cast<Real>(acf_dt_));
        if (!r.valid) {
            // Same reasons the command line reports: too few samples, too short
            // a span, or a constant signal.
            spectral_note_ += "  " + ch[i].name +
                              ": not enough or constant data - skipped\n";
            continue;
        }
        spectra_.push_back({ch[i].name, ch[i].unit, std::move(r)});
    }
}

// The GUI and the CLI's --acf-* flags run the same signal::compute_acf_psd over
// the same channels::extract output, so their tables should agree to the last
// digit. This writes the GUI's side in that same format so the claim can be
// checked with a diff rather than by eye.
void AppWindow::dump_spectra() {
    if (spectra_.empty()) return;

    const std::string dir = paths::directory_of(path_);

    std::string note;
    for (const Spectrum& s : spectra_) {
        const std::string out = dir + s.name + ".gui.acf.dat";
        note += io::write_spectral_file(out, s.name, s.unit, s.result)
                    ? "  wrote " + out + "\n"
                    : "  could not write " + out + "\n";
    }
    spectral_note_ = note;
}

// The channel picker and the Compute button, shown at the top of both spectral
// tabs so the selection can be changed from either. Returns the width available
// for the plots below.
// Load a spectrum the command line wrote, and plot its columns as they stand.
// This is the escape hatch for any spectrum the GUI cannot reproduce: the CLI
// run may have used a wind term, a different mass or CdA, or simply an older
// build. Nothing is recomputed here.
void AppWindow::draw_dat_loader() {
    ImGui::SetNextItemWidth(360.0f);
    const bool entered = ImGui::InputTextWithHint(
        "##datpath", "path to a .acf.dat written by --acf-*, then press Enter",
        dat_path_, IM_ARRAYSIZE(dat_path_), ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::SameLine();
    const bool clicked = ImGui::Button("Plot file");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Plot columns 3 and 4 of the file exactly as written.\n"
                          "Adds to whatever is already shown, so a recomputed\n"
                          "channel and the file's version can be compared.");

    if (!(entered || clicked) || dat_path_[0] == '\0') return;

    Spectrum    s;
    std::string err;
    if (!load_spectrum_file(dat_path_, s, err)) {
        spectral_note_ = "  " + err + "\n";
        return;
    }
    spectra_.push_back(std::move(s));
    psd_refit_     = true;
    spectral_note_ = "  loaded " + std::string(dat_path_) + " (plotted as-is)\n";
}

float AppWindow::draw_spectral_controls() {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float page_w = ImGui::GetContentRegionAvail().x - style.ScrollbarSize;

    // Offered even with no GPX loaded: a .dat file stands on its own.
    draw_dat_loader();

    if (path_.empty()) {
        ImGui::TextDisabled("Load a GPX activity first.");
        return page_w;
    }
    if (result_.tracks.empty() ||
        result_.tracks[static_cast<Size>(track_)].channels.empty()) {
        ImGui::TextDisabled("This track carries no channels to transform.");
        return page_w;
    }

    draw_channel_picker();
    draw_transform_controls();

    if (!spectral_note_.empty())
        theme::text_coloured(theme::kWarning, spectral_note_);

    ImGui::Separator();
    return page_w;
}

// One checkbox per channel the track carries, on a single line.
void AppWindow::draw_channel_picker() {
    const std::vector<channels::Channel>& ch =
        result_.tracks[static_cast<Size>(track_)].channels;
    if (chan_on_.size() != ch.size()) reset_channel_selection();

    ImGui::TextUnformatted("Channels:");
    for (Size i = 0; i < ch.size(); ++i) {
        ImGui::SameLine();
        bool on = chan_on_[i] != 0;
        if (ImGui::Checkbox(ch[i].name.c_str(), &on)) chan_on_[i] = on ? 1 : 0;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s in %s, %zu samples",
                              ch[i].name.c_str(), ch[i].unit.c_str(),
                              ch[i].t_s.size());
    }
}

// The resample interval, the button that runs the transform, and what came of it.
void AppWindow::draw_transform_controls() {
    // Mirrors the CLI's --acf-dt: 0 lets the transform pick the median interval.
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("resample dt (s), 0 = auto", &acf_dt_, 0.0f, 0.0f, "%.2f");
    if (acf_dt_ < 0.0f) acf_dt_ = 0.0f;

    ImGui::SameLine();
    if (ImGui::Button("Compute")) compute_spectra();

    if (spectra_.empty()) return;

    ImGui::SameLine();
    if (ImGui::Button("Dump to .dat")) dump_spectra();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Write these spectra beside the GPX as\n"
                          "<channel>.gui.acf.dat, in the same format the\n"
                          "command line's --acf-* flags produce.");

    ImGui::SameLine();
    ImGui::TextDisabled("(%zu computed at dt = %.3g s)", spectra_.size(),
                        spectra_.front().result.dt_s);
}

void AppWindow::draw_acf_tab() {
    const float page_w = draw_spectral_controls();
    if (spectra_.empty()) {
        if (!path_.empty()) ImGui::TextDisabled(
            "Pick one or more channels and press Compute.");
        return;
    }

    ImGui::Checkbox("show the whole lag range", &full_lag_);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Off: the first 10 minutes of lag, where the structure is.\n"
                          "On: every lag out to the length of the ride.");

    const float h = std::max(240.0f, ImGui::GetContentRegionAvail().y - 12.0f);
    draw_acf_plot(spectra_, full_lag_, page_w, h);
}

void AppWindow::draw_psd_tab() {
    const float page_w = draw_spectral_controls();
    if (spectra_.empty()) {
        if (!path_.empty()) ImGui::TextDisabled(
            "Pick one or more channels and press Compute.");
        return;
    }

    ImGui::BeginChild("psds", ImVec2(0.0f, 0.0f));
    draw_psd_plots(spectra_, psd_refit_, page_w - 12.0f, 230.0f);
    psd_refit_ = false;   // the new data now owns the axes
    ImGui::EndChild();
}

// The zone chart, in a panel of its own. `width` is passed in rather than taken
// from the content region: the report page scrolls horizontally for the wide
// text tables, and the panel should follow the visible width, not that.
void AppWindow::draw_zone_panel(float width) {
    if (result_.tracks.empty()) return;

    const zones::ZoneTable& table =
        result_.tracks[static_cast<Size>(track_)].power_zones;
    const float height = zone_chart_height(table) + PanelScope::chrome_height() +
                         ImGui::GetStyle().ItemSpacing.y * 2.0f;

    const PanelScope panel("zones", ImVec2(width, height));
    draw_zone_chart(table);
}

void AppWindow::draw_peaks_panel(float width) {
    if (result_.tracks.empty()) return;

    const TrackCharts& tc = result_.tracks[static_cast<Size>(track_)];
    if (tc.peaks.empty()) return;

    const float height = peaks_chart_height(tc.peaks) +
                         hold_curve_height() +
                         ImGui::GetTextLineHeightWithSpacing() * 2.0f +
                         PanelScope::chrome_height() +
                         ImGui::GetStyle().ItemSpacing.y * 4.0f + 6.0f;

    const PanelScope panel("peaks", ImVec2(width, height));
    draw_peaks_chart(tc.peaks, tc.power_zones);

    // The same efforts read the other way round: how long each fraction of the
    // power was sustained.
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::TextUnformatted("How long the power was held      relative to:");
    ImGui::SameLine();
    if (ImGui::RadioButton("best effort", hold_ref_ == HoldRef::Peak))
        hold_ref_ = HoldRef::Peak;
    ImGui::SameLine();
    if (tc.ftp_w <= 0.0) ImGui::BeginDisabled();
    if (ImGui::RadioButton("FTP", hold_ref_ == HoldRef::Ftp))
        hold_ref_ = HoldRef::Ftp;
    if (tc.ftp_w <= 0.0) ImGui::EndDisabled();

    draw_hold_curve(tc.peaks, tc.ftp_w, hold_ref_,
                    ImGui::GetContentRegionAvail().x - kAxisLabelGutter);
}

// One elevation profile per detected climb, with a shared x-axis choice.
void AppWindow::draw_hill_panel(float width) {
    if (result_.tracks.empty()) return;

    const std::vector<HillProfile>& hills =
        result_.tracks[static_cast<Size>(track_)].hills;
    if (hills.empty()) return;

    // Whether any climb has timestamps at all decides if "time" is offerable.
    bool any_timed = false;
    for (const HillProfile& h : hills)
        if (!h.time_min.empty()) { any_timed = true; break; }

    const float rows   = static_cast<float>(hills.size());
    const float height = ImGui::GetTextLineHeightWithSpacing() * 2.0f +
                         hill_chart_height() * rows +
                         ImGui::GetStyle().ItemSpacing.y * rows +
                         PanelScope::chrome_height() + 2.0f;

    const PanelScope panel("hills", ImVec2(width, height));

    ImGui::TextUnformatted("Climb profiles      x axis:");
    ImGui::SameLine();
    if (ImGui::RadioButton("distance", hill_axis_ == HillAxis::Distance))
        hill_axis_ = HillAxis::Distance;
    ImGui::SameLine();
    if (!any_timed) ImGui::BeginDisabled();
    if (ImGui::RadioButton("time", hill_axis_ == HillAxis::Time))
        hill_axis_ = HillAxis::Time;
    if (!any_timed) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("(no timestamps on these climbs)");
    }

    // The fills are coloured by power zone, and the zone table itself is further
    // down the report, so the key belongs here.
    const zones::ZoneTable& zt = result_.tracks[static_cast<Size>(track_)].power_zones;
    if (zt.valid) {
        ImGui::TextUnformatted("shading = power in section:");
        draw_zone_legend(zt);
    }

    // Leave room for the y-axis labels ImPlot draws outside the plot frame.
    const float plot_w = ImGui::GetContentRegionAvail().x - kAxisLabelGutter;
    for (Size i = 0; i < hills.size(); ++i)
        draw_hill_chart(hills[i], static_cast<int>(i), hill_axis_, zt, plot_w);
}

void AppWindow::draw_toolbar() {
    if (file_dialog_available()) {
        if (ImGui::Button("Load GPX...")) {
            const std::string chosen = open_gpx_file(start_dir_);
            if (!chosen.empty()) load(chosen);
        }
    } else {
        // No zenity on this system: let the path be typed instead.
        static char typed[1024] = "";
        ImGui::SetNextItemWidth(360.0f);
        const bool entered = ImGui::InputTextWithHint(
            "##path", "path to a .gpx file, then press Enter", typed,
            IM_ARRAYSIZE(typed), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Load") || entered) && typed[0] != '\0')
            load(typed);
    }

    ImGui::SameLine();
    if (path_.empty()) {
        ImGui::TextDisabled("no file loaded - drag a .gpx file here, or use the button");
        return;
    }

    ImGui::Text("%s", paths::basename_of(path_).c_str());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path_.c_str());

    ImGui::SameLine();
    if (ImGui::Button("Reload")) load(path_);

    ImGui::SameLine();
    if (ImGui::Button("Copy report"))
        ImGui::SetClipboardText(result_.summary.c_str());

    // Mirrors the CLI's --points: how many track points the report lists first.
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::InputInt("track points", &max_print_)) {
        if (max_print_ < 0) max_print_ = 0;
        load(path_);   // re-run so the report reflects the new setting
    }

    // Mirrors the command line's --wind. Off by default because it is the only
    // control that reaches the network; ticking it re-runs the analysis, since
    // the headwind term changes estimated power and all that follows from it.
    ImGui::SameLine();
    if (ImGui::Checkbox("wind", &wind_on_)) load(path_);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Fetch historical wind from Open-Meteo and apply it to\n"
                          "the aero term, as the command line's --wind does.\n"
                          "Leaves the network alone while unticked.");
}

void AppWindow::draw_banner() {
    if (result_.errors.empty()) return;

    const ImVec4 colour = result_.ok ? theme::kWarning : theme::kError;
    ImGui::PushStyleColor(ImGuiCol_Text, colour);
    ImGui::TextUnformatted(result_.errors.c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
}

// The report page: the full text report in one scrolling view, with each chart
// placed directly below the table it illustrates.
void AppWindow::draw_report() {
    if (path_.empty()) {
        ImGui::TextDisabled(
            "Load a GPX activity to see its analysis report here.");
        return;
    }

    ImGui::BeginChild("page", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);

    // Visible width of this page, independent of how wide the text makes the
    // scrollable content.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float page_w = ImGui::GetWindowWidth() - style.WindowPadding.x * 2.0f -
                         style.ScrollbarSize;

    draw_track_selector();

    // The report is column-aligned plain text (hills, splits and peak tables),
    // so it is shown verbatim in ImGui's fixed-width default font. Charts are
    // inserted directly below the table each one illustrates.
    if (result_.summary.empty()) {
        ImGui::TextDisabled("No report was produced for this file.");
        ImGui::EndChild();
        return;
    }

    const std::string& r = result_.summary;

    std::size_t pos = 0;
    for (const ChartCut& cut : find_chart_cuts(r)) {
        ImGui::TextUnformatted(r.c_str() + pos, r.c_str() + cut.at);
        switch (cut.chart) {
            case ReportChart::Hills: draw_hill_panel(page_w);  break;
            case ReportChart::Zones: draw_zone_panel(page_w);  break;
            case ReportChart::Peaks: draw_peaks_panel(page_w); break;
        }
        pos = cut.at;
    }
    ImGui::TextUnformatted(r.c_str() + pos, r.c_str() + r.size());

    ImGui::EndChild();
}

// A file with several tracks gets one selector driving every chart on the page.
void AppWindow::draw_track_selector() {
    if (result_.tracks.size() <= 1) {
        track_ = 0;
        return;
    }
    if (track_ >= static_cast<int>(result_.tracks.size())) track_ = 0;

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderInt("track (charts)", &track_, 0,
                     static_cast<int>(result_.tracks.size()) - 1);
    ImGui::Spacing();
}

} // namespace gui
