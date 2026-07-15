#pragma once

#include <string>

namespace cppcoder {

// Ollama models routinely wrap JSON in prose, markdown fences, or
// reasoning text despite being asked not to. These pull the outermost
// {...} or [...] span out of raw model output so it can be parsed.
// Both return an empty string if no matching bracket pair is found.
std::string ExtractJsonObject(const std::string& raw);
std::string ExtractJsonArray(const std::string& raw);

}  // namespace cppcoder
