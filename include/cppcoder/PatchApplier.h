#pragma once

#include "cppcoder/Types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace cppcoder {

struct ApplyOutcome {
    std::vector<std::string> writtenPaths;
    std::vector<std::string> rejectedPaths;  // failed the path-safety check
    std::vector<std::string> errors;         // "path: message" for I/O failures
};

// Writes ProposedEdit content to disk under a fixed root, rejecting any
// edit whose resolved path would escape that root (an LLM-supplied path
// is untrusted input; CodebaseScanner never needs this check since its
// reads can't escape root by construction, but a writer must guard it
// explicitly).
class PatchApplier {
public:
    explicit PatchApplier(std::filesystem::path root);

    ApplyOutcome Apply(const std::vector<ProposedEdit>& edits) const;

private:
    bool IsPathSafe(const std::filesystem::path& resolved) const;

    std::filesystem::path root_;
};

}  // namespace cppcoder
