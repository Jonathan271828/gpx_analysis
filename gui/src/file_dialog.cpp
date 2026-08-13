#include "file_dialog.hpp"

#include "paths.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

// Run a command and return everything it wrote to stdout.
std::string output_of(const std::string& cmd) {
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) return {};

    std::string out;
    std::array<char, 4096> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        out += buf.data();
    ::pclose(pipe);
    return out;
}

// Split zenity's reply. With --multiple it separates paths with the character
// given to --separator, and always ends with a newline.
std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string              current;
    for (const char c : text) {
        if (c == separator || c == '\n') {
            if (!current.empty()) parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) parts.push_back(current);
    return parts;
}

// A path may legally contain any character but '/' and NUL, so the separator is
// one that a chooser will not produce in a name.
constexpr char kSeparator = '\n';

} // namespace

bool file_dialog_available() {
    static const bool available = have_program("zenity");
    return available;
}

std::vector<std::string> open_gpx_files(std::string& start_dir) {
    if (!file_dialog_available()) return {};

    // Trailing slash tells zenity to treat --filename as a folder, not a name.
    std::string cmd = "zenity --file-selection --multiple --separator=" +
                      shell_quote(std::string(1, kSeparator)) +
                      " --title=" + shell_quote("Open GPX activities") +
                      " --file-filter=" + shell_quote("GPX activities | *.gpx *.GPX") +
                      " --file-filter=" + shell_quote("All files | *");
    if (!start_dir.empty())
        cmd += " --filename=" + shell_quote(start_dir);
    cmd += " 2>/dev/null";

    std::vector<std::string> paths = split(output_of(cmd), kSeparator);
    if (paths.empty()) return {};   // cancelled

    const std::string dir = paths::directory_of(paths.front());
    if (!dir.empty()) start_dir = dir;
    return paths;
}

} // namespace gui
