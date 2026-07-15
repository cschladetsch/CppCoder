#pragma once

#include "cppcoder/CodebaseScanner.h"
#include "cppcoder/Judge.h"
#include "cppcoder/OllamaClient.h"
#include "cppcoder/TaskQueue.h"
#include "cppcoder/Types.h"
#include "cppcoder/Worker.h"

#include <chrono>
#include <functional>
#include <vector>

namespace cppcoder {

struct EngineConfig {
    std::size_t tokenBudgetPerTask = 120'000;
    int maxIterations = 200;
    std::chrono::minutes maxWallClock{90};  // matches the 1.5h longest test in the spec
    std::size_t maxInitialKeywords = 5;
};

struct ResearchResult {
    bool answered = false;
    std::string answer;
    std::vector<Finding> successfulFindings;
    int iterationsRun = 0;
    std::chrono::milliseconds wallClock{0};
    std::string terminationReason;
};

// Receives one already-serialized JSON object (no trailing newline) per
// engine event: {"event":"keywords_extracted",...}, {"event":"task_queued",...},
// {"event":"task_started",...}, {"event":"worker_result",...},
// {"event":"judge_result",...}, {"event":"complete",...}. Intended to be
// written as JSON Lines for the web UI (see web/index.html) to replay.
using EventSink = std::function<void(const std::string&)>;

// Pure keyword fallback used when the model's own keyword extraction
// fails or returns nothing usable. Public/free so it is directly
// unit testable.
std::vector<std::string> FallbackKeywords(const std::string& question, std::size_t maxCount);

// Orchestrates the full sustained-research loop described in the spec:
//  1. Extract keywords from the question and probe for initial
//     research directions.
//  2. Build the first task and run the main worker -> judge loop,
//     never revisiting an area and dropping off-topic directions.
//  3. Synthesize a final answer from the accumulated successful
//     findings once the queue drains or a limit is hit.
class ResearchEngine {
public:
    ResearchEngine(OllamaClient client, CodebaseScanner scanner, EngineConfig config = {});

    void SetEventSink(EventSink sink) { eventSink_ = std::move(sink); }

    ResearchResult Research(const std::string& question);

    // Greps the codebase for the given keywords and, if anything
    // matches, returns a single repeatable seed Task covering those
    // areas. Does not call the model, so it is safe to unit test
    // without a running Ollama instance.
    std::vector<Task> SeedInitialTasks(const std::string& question,
                                        const std::vector<std::string>& keywords) const;

private:
    OllamaClient client_;
    CodebaseScanner scanner_;
    EngineConfig config_;
    Worker worker_;
    Judge judge_;
    EventSink eventSink_;

    std::vector<std::string> ExtractKeywords(const std::string& question) const;
    std::string SynthesizeAnswer(const std::string& question,
                                  const std::vector<Finding>& successfulFindings) const;
    void Emit(const std::string& jsonLine) const;
};

}  // namespace cppcoder

