#pragma once

#include "cppcoder/CodebaseScanner.h"
#include "cppcoder/Editor.h"
#include "cppcoder/OllamaClient.h"
#include "cppcoder/PatchApplier.h"
#include "cppcoder/TaskQueue.h"
#include "cppcoder/Types.h"

#include <chrono>
#include <filesystem>
#include <vector>

namespace cppcoder {

struct EditEngineConfig {
    std::size_t tokenBudgetPerTask = 120'000;
    int maxIterations = 200;
    std::chrono::minutes maxWallClock{90};
    std::size_t maxInitialKeywords = 5;
    bool apply = false;  // dry-run unless explicitly enabled
};

struct EditRunResult {
    bool anyEdits = false;
    std::vector<ProposedEdit> proposedEdits;  // populated when apply == false (dry-run)
    ApplyOutcome applyOutcome;                 // populated when apply == true
    int iterationsRun = 0;
    std::chrono::milliseconds wallClock{0};
    std::string terminationReason;
};

// Orchestrates the edit-mode loop: seed tasks from the task description's
// keywords (same keyword-probe approach as ResearchEngine), then run a
// worker-only loop (Editor -> outcome dispatch, no judge step) that
// either accumulates proposed edits (dry-run) or applies each one to
// disk immediately as it's produced, so later tasks in the same run see
// already-written content when they re-scan.
class EditEngine {
public:
    EditEngine(OllamaClient client, CodebaseScanner scanner, std::filesystem::path root,
               EditEngineConfig config = {});

    void SetEventSink(EventSink sink) { eventSink_ = std::move(sink); }

    EditRunResult Run(const std::string& taskDescription);

    // Greps the codebase for the given keywords and, if anything
    // matches, returns a single repeatable seed Task covering those
    // areas. Does not call the model, so it is safe to unit test
    // without a running Ollama instance.
    std::vector<Task> SeedInitialTasks(const std::string& taskDescription,
                                        const std::vector<std::string>& keywords) const;

private:
    OllamaClient client_;
    CodebaseScanner scanner_;
    std::filesystem::path root_;
    EditEngineConfig config_;
    Editor editor_;
    PatchApplier applier_;
    EventSink eventSink_;

    std::vector<std::string> ExtractKeywords(const std::string& taskDescription) const;
    void Emit(const std::string& jsonLine) const;
};

}  // namespace cppcoder
