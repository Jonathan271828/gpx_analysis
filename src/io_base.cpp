#include "io_base.hpp"

#include <sstream>

namespace io {

std::string format_duration(Long seconds) {
    Long h = seconds / 3600;
    Long m = (seconds % 3600) / 60;
    Long s = seconds % 60;
    std::ostringstream oss;
    if (h > 0) oss << h << "h ";
    if (h > 0 || m > 0) oss << m << "m ";
    oss << s << "s";
    return oss.str();
}

} // namespace io
