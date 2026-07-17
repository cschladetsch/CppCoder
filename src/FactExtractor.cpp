#include "cppcoder/FactExtractor.h"

#include <regex>

namespace cppcoder {

namespace {

std::string FormatUserName(const std::smatch& m) { return "The user's name is " + m[1].str() + "."; }

std::string FormatAssistantName(const std::smatch& m) {
    return "The assistant should be called " + m[1].str() + ".";
}

std::string FormatAge(const std::smatch& m) { return "The user is " + m[1].str() + " years old."; }

struct FactPattern {
    std::regex pattern;
    std::string (*format)(const std::smatch&);
};

// Order matters where patterns could otherwise both fire on the same
// clause (they currently don't overlap), but doesn't matter for which
// facts get found overall -- every pattern is tried against the full
// message. Add new phrasings here as they come up; keep each pattern
// narrow rather than trying to build one do-everything regex.
const std::vector<FactPattern>& Patterns() {
    static const std::vector<FactPattern> patterns = {
        // "My name is Christian" / "My name Christian" / "My name's Christian"
        {std::regex(R"(\bmy name(?:'s|\s+is)?\s+([A-Za-z][\w'-]*))", std::regex::icase),
         &FormatUserName},
        // "Your name is Charlie" -- the user telling the assistant its own name.
        {std::regex(R"(\byour name(?:'s|\s+is)?\s+([A-Za-z][\w'-]*))", std::regex::icase),
         &FormatAssistantName},
        // "I am 55yo" / "I'm 55 years old" / "I am 55 yo"
        {std::regex(R"(\bi(?:'m|\s+am)\s+(\d{1,3})\s*(?:yo\b|years?\s*old\b))",
                    std::regex::icase),
         &FormatAge},
    };
    return patterns;
}

}  // namespace

std::vector<std::string> ExtractFacts(const std::string& message) {
    std::vector<std::string> facts;
    for (const auto& p : Patterns()) {
        std::smatch m;
        if (std::regex_search(message, m, p.pattern)) {
            facts.push_back(p.format(m));
        }
    }
    return facts;
}

}  // namespace cppcoder
