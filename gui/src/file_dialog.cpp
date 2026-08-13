#include "file_dialog.hpp"

#include "paths.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace gui {

namespace {

// Wrap a string in single quotes for the shell, escaping any it contains.
std::string shell_quote(const std::string& s) {
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'') out += "'\\''";
        else           out += c;
    }
    out += "'";
    return out;
}

// Whether a program exists on PATH.
bool have_program(const char* name) {
    const std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

// Run a command and return its first line of stdout, without the newline.
std::string first_line_of(const std::string& cmd) {
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) return {};

    std::string out;
    std::array<char, 4096> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        out += buf.data();
    ::pclose(pipe);

    const std::string::size_type nl = out.find('\n');
    if (nl != std::string::npos) out.erase(nl);
    return out;
}

} // namespace

bool file_dialog_available() {
    static const bool available = have_program("zenity");
    return available;
}

std::string open_gpx_file(std::string& start_dir) {
    if (!file_dialog_available()) return {};

    // Trailing slash tells zenity to treat --filename as a folder, not a name.
    std::string cmd = "zenity --file-selection --title=" +
                      shell_quote("Open a GPX activity") +
                      " --file-filter=" + shell_quote("GPX activities | *.gpx *.GPX") +
                      " --file-filter=" + shell_quote("All files | *");
    if (!start_dir.empty())
        cmd += " --filename=" + shell_quote(start_dir);
    cmd += " 2>/dev/null";

    const std::string path = first_line_of(cmd);
    if (path.empty()) return {};   // cancelled

    const std::string dir = paths::directory_of(path);
    if (!dir.empty()) start_dir = dir;
    return path;
}

} // namespace gui
