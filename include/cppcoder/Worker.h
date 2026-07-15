#pragma once

#include "cppcoder/CodebaseScanner.h"
#include "cppcoder/OllamaClient.h"
#include "cppcoder/Types.h"

namespace cppcoder {

// Executes a single research task: gathers context for the task's
// target area (bounded by a token budget), asks the model to search it
// against the research goal / success criteria, and parses the result
// into one of the three outcomes described in the spec: Success,
// NoInformation, or PartialWithDirections.
class Worker {
public:
    Worker(const OllamaClient& client, const CodebaseScanner& scanner,
           std::size_t tokenBudget = 120'000);

    Finding Execute(const Task& task) const;

    // Pure, side-effect-free parse of a worker model response into a
    // Finding. Public and static so it can be unit tested directly
    // without a running Ollama instance.
    static Finding ParseWorkerResponse(const std::string& raw, const std::string& area);

private:
    const OllamaClient& client_;
    const CodebaseScanner& scanner_;
    std::size_t tokenBudget_;

    Finding ExecuteSingleArea(const Task& task, const std::string& area) const;
    std::string BuildPrompt(const Task& task, const ScanResult& scan) const;
};

}  // namespace cppcoder
