#pragma once

#include <string>
#include <vector>

namespace cppcoder {

// Lightweight, dependency-free heuristics for pulling durable facts out
// of a single chat message -- "My name is Christian", "I am 55yo",
// "Your name is Charlie" and similar first-person statements. This is
// deliberately a handful of regexes rather than an LLM call or a full
// NLP pipeline: it costs nothing extra per message and covers the
// common phrasings; add patterns to FactExtractor.cpp as new ones come
// up rather than trying to generalize up front.
//
// Returns normalized fact sentences suitable for storing verbatim in
// MemoryStore (e.g. "The user's name is Christian."). Returns an empty
// vector if nothing matched.
std::vector<std::string> ExtractFacts(const std::string& message);

}  // namespace cppcoder
