#include "cppcoder/PatchApplier.h"

#include <spdlog/spdlog.h>

#include <fstream>

namespace cppcoder {

namespace fs = std::filesystem;

PatchApplier::PatchApplier(std::filesystem::path root) : root_(std::move(root)) {}

bool PatchApplier::IsPathSafe(const fs::path& resolved) const {
    fs::path canonicalRoot = fs::weakly_canonical(root_);
    fs::path canonicalResolved = fs::weakly_canonical(resolved);

    auto rootIt = canonicalRoot.begin();
    auto resolvedIt = canonicalResolved.begin();
    for (; rootIt != canonicalRoot.end(); ++rootIt, ++resolvedIt) {
        if (resolvedIt == canonicalResolved.end() || *resolvedIt != *rootIt) {
            return false;
        }
    }
    return true;
}

ApplyOutcome PatchApplier::Apply(const std::vector<ProposedEdit>& edits) const {
    ApplyOutcome outcome;

    for (const auto& edit : edits) {
        if (edit.path.empty()) {
            outcome.rejectedPaths.push_back(edit.path);
            continue;
        }

        fs::path candidate(edit.path);
        if (candidate.is_absolute()) {
            // fs::path's operator/ discards the left-hand side entirely
            // when the right-hand side is absolute, so an absolute
            // edit.path must be rejected before it ever reaches root_/candidate.
            spdlog::warn("PatchApplier: rejecting absolute edit path '{}'", edit.path);
            outcome.rejectedPaths.push_back(edit.path);
            continue;
        }

        fs::path resolved = root_ / candidate;
        if (!IsPathSafe(resolved)) {
            spdlog::warn("PatchApplier: rejecting edit path '{}' -- escapes codebase root",
                         edit.path);
            outcome.rejectedPaths.push_back(edit.path);
            continue;
        }

        std::error_code ec;
        fs::path parent = resolved.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent, ec);
            if (ec) {
                outcome.errors.push_back(edit.path + ": " + ec.message());
                continue;
            }
        }

        std::ofstream file(resolved, std::ios::binary | std::ios::trunc);
        if (!file) {
            outcome.errors.push_back(edit.path + ": could not open for writing");
            continue;
        }
        file << edit.newContent;
        if (!file) {
            outcome.errors.push_back(edit.path + ": write failed");
            continue;
        }

        outcome.writtenPaths.push_back(edit.path);
        spdlog::info("PatchApplier: wrote {}", edit.path);
    }

    return outcome;
}

}  // namespace cppcoder
