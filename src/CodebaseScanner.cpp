#include "cppcoder/CodebaseScanner.h"
#include "cppcoder/Types.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace cppcoder {

CodebaseScanner::CodebaseScanner(fs::path root, std::vector<std::string> extensions)
    : root_(std::move(root)), extensions_(std::move(extensions)) {}

bool CodebaseScanner::HasTrackedExtension(const fs::path& p) const {
    const std::string ext = p.extension().string();
    return std::find(extensions_.begin(), extensions_.end(), ext) != extensions_.end();
}

ScanResult CodebaseScanner::Scan(const std::string& targetArea, std::size_t tokenBudget) const {
    ScanResult result;
    fs::path start = targetArea.empty() ? root_ : root_ / targetArea;

    std::vector<fs::path> candidates;
    std::error_code ec;
    if (fs::is_regular_file(start, ec)) {
        candidates.push_back(start);
    } else if (fs::is_directory(start, ec)) {
        for (auto it = fs::recursive_directory_iterator(
                 start, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); ++it) {
            if (ec) break;
            const auto& entry = *it;
            if (entry.path().string().find("/.git/") != std::string::npos) continue;
            if (entry.path().string().find("/build/") != std::string::npos) continue;
            if (entry.is_regular_file(ec) && HasTrackedExtension(entry.path())) {
                candidates.push_back(entry.path());
            }
        }
    }
    std::sort(candidates.begin(), candidates.end());

    std::ostringstream out;
    std::size_t tokensUsed = 0;
    for (const auto& file : candidates) {
        std::ifstream in(file, std::ios::binary);
        if (!in) continue;
        std::ostringstream contentStream;
        contentStream << in.rdbuf();
        std::string content = contentStream.str();

        std::string relPath = fs::relative(file, root_, ec).string();
        std::string header = "\n// ==== " + relPath + " ====\n";
        std::size_t entryTokens = EstimateTokens(header) + EstimateTokens(content);

        if (tokensUsed + entryTokens > tokenBudget) {
            result.filesSkippedBudget.push_back(relPath);
            continue;
        }

        out << header << content;
        tokensUsed += entryTokens;
        result.filesIncluded.push_back(relPath);
    }

    result.concatenatedContent = out.str();
    result.approxTokens = tokensUsed;
    return result;
}

std::vector<std::string> CodebaseScanner::FindFilesMatchingKeyword(const std::string& keyword,
                                                                     std::size_t maxResults) const {
    std::vector<std::string> matches;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(
             root_, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ++it) {
        if (ec) break;
        const auto& entry = *it;
        if (entry.path().string().find("/.git/") != std::string::npos) continue;
        if (entry.path().string().find("/build/") != std::string::npos) continue;
        if (!entry.is_regular_file(ec) || !HasTrackedExtension(entry.path())) continue;

        std::ifstream in(entry.path(), std::ios::binary);
        if (!in) continue;
        std::ostringstream contentStream;
        contentStream << in.rdbuf();
        std::string content = contentStream.str();

        auto lowerFind = [](std::string haystack, std::string needle) {
            std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
            std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
            return haystack.find(needle) != std::string::npos;
        };

        if (lowerFind(content, keyword) || lowerFind(entry.path().filename().string(), keyword)) {
            matches.push_back(fs::relative(entry.path(), root_, ec).string());
            if (matches.size() >= maxResults) break;
        }
    }
    return matches;
}

}  // namespace cppcoder
