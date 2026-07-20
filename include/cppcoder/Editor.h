#pragma once

#include "cppcoder/CodebaseScanner.h"
#include "cppcoder/OllamaClient.h"
#include "cppcoder/Types.h"

namespace cppcoder {

// Executes a single edit task: gathers context for the task's target
// area (bounded by a token budget), asks the model to propose a change
// against the task's goal / success criteria, and parses the result
// into one of three outcomes: Success, NoChangeNeeded, or
// PartialWithDirections. Mirrors Worker, but the model is asked to
// return full replacement file content instead of a research summary.
class Editor {
public:
    Editor(const OllamaClient& client, const CodebaseScanner& scanner,
           std::size_t tokenBudget = 120'000);

    EditFinding Execute(const Task& task) const;

    // Pure, side-effect-free parse of an editor model response into an
    // EditFinding. Public and static so it can be unit tested directly
    // without a running Ollama instance.
    static EditFinding ParseEditResponse(const std::string& raw, const std::string& area);

private:
    const OllamaClient& client_;
    const CodebaseScanner& scanner_;
    std::size_t tokenBudget_;

    EditFinding ExecuteSingleArea(const Task& task, const std::string& area) const;
    std::string BuildPrompt(const Task& task, const ScanResult& scan) const;
};

}  // namespace cppcoder
