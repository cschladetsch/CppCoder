#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cppcoder {

struct ScanResult {
    std::string concatenatedContent;  // file contents, each prefixed with a path header
    std::vector<std::string> filesIncluded;
    std::vector<std::string> filesSkippedBudget;  // matched but excluded by token budget
    std::size_t approxTokens = 0;
};

// Reads source files under a root/target area, respecting an approximate
// token budget so a single worker call stays within the usable context
// window (empirically ~100-150K tokens for local models).
class CodebaseScanner {
public:
    explicit CodebaseScanner(std::filesystem::path root,
                              std::vector<std::string> extensions = {
                                  ".cpp", ".h", ".hpp", ".cc", ".cxx", ".py", ".rs", ".scala"});

    // `targetArea` is a path relative to root (file or directory). An
    // empty string scans the whole root.
    ScanResult Scan(const std::string& targetArea, std::size_t tokenBudget) const;

    // Cheap keyword grep across the whole tree, used to seed the initial
    // research directions from the question's keywords.
    std::vector<std::string> FindFilesMatchingKeyword(const std::string& keyword,
                                                        std::size_t maxResults = 20) const;

private:
    std::filesystem::path root_;
    std::vector<std::string> extensions_;

    bool HasTrackedExtension(const std::filesystem::path& p) const;
};

}  // namespace cppcoder
