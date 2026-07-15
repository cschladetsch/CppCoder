#pragma once

#include <string>

namespace cppcoder {

// Configures the default spdlog logger used throughout cppcoder: a
// colored console sink, plus an optional file sink. Safe to call more
// than once (e.g. in tests); later calls replace the previous config.
//
// `levelName` accepts spdlog's usual names: trace, debug, info, warn,
// err, critical, off. Unrecognised names fall back to "info".
void InitLogging(const std::string& levelName = "info", const std::string& logFilePath = "");

}  // namespace cppcoder
