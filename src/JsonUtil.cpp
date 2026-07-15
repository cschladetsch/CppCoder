#include "cppcoder/JsonUtil.h"

namespace cppcoder {

std::string ExtractJsonObject(const std::string& raw) {
    auto start = raw.find('{');
    auto end = raw.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end < start) {
        return "";
    }
    return raw.substr(start, end - start + 1);
}

std::string ExtractJsonArray(const std::string& raw) {
    auto start = raw.find('[');
    auto end = raw.rfind(']');
    if (start == std::string::npos || end == std::string::npos || end < start) {
        return "";
    }
    return raw.substr(start, end - start + 1);
}

}  // namespace cppcoder
