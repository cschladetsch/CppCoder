#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cppcoder {

// A unit of research work: a target area to investigate, a goal, and
// success criteria that let a worker (and later the judge) know when
// the task has actually been satisfied.
struct Task {
    std::string id;
    std::string targetArea;       // directory / file glob / symbol scope
    std::string researchGoal;     // what we are trying to find out
    std::string successCriteria;  // definition of "done" for this task
    int depth = 0;                // hops away from the original question
    bool repeatable = false;      // apply the same goal across many areas
    std::vector<std::string> repeatTargets;  // sub-areas for repeatable tasks
    std::string parentId;         // id of the task whose finding suggested this one
};

enum class WorkerOutcome {
    Success,               // required info found and extracted
    NoInformation,         // area contains nothing relevant
    PartialWithDirections  // some/no info, but plausible leads found
};

struct Finding {
    WorkerOutcome outcome = WorkerOutcome::NoInformation;
    std::string areaInvestigated;
    std::string summary;                 // compact extracted findings
    std::vector<Task> suggestedDirections;
    std::chrono::milliseconds duration{0};
    std::size_t promptTokensApprox = 0;
};

enum class EditOutcome {
    Success,               // edits[] contains the complete change for this area
    NoChangeNeeded,        // area investigated, nothing to change here
    PartialWithDirections  // edits[] may be non-empty; more areas need changes too
};

struct ProposedEdit {
    std::string path;         // relative to codebase root
    std::string newContent;   // full replacement content for the file
    std::string description;  // short human-readable summary of the change
};

struct EditFinding {
    EditOutcome outcome = EditOutcome::NoChangeNeeded;
    std::string areaInvestigated;
    std::string summary;
    std::vector<ProposedEdit> edits;
    std::vector<Task> suggestedDirections;
    std::chrono::milliseconds duration{0};
    std::size_t promptTokensApprox = 0;
};

// Receives one already-serialized JSON object (no trailing newline) per
// engine event, e.g. {"event":"task_queued",...}. Shared by ResearchEngine
// and EditEngine. Intended to be written as JSON Lines for the web UI
// (see web/index.html) to replay.
using EventSink = std::function<void(const std::string&)>;

// Approximate token estimate. Good enough for budgeting against the
// empirical 100-150K usable context window; not a real tokenizer.
inline std::size_t EstimateTokens(std::string_view text) {
    return text.size() / 4 + 1;
}

}  // namespace cppcoder
