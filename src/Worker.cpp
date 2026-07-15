#include "cppcoder/Worker.h"

#include "cppcoder/JsonUtil.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <sstream>

namespace cppcoder {

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

namespace {

int TaskCounter() {
    static int counter = 0;
    return ++counter;
}

}  // namespace

Worker::Worker(const OllamaClient& client, const CodebaseScanner& scanner,
               std::size_t tokenBudget)
    : client_(client), scanner_(scanner), tokenBudget_(tokenBudget) {}

std::string Worker::BuildPrompt(const Task& task, const ScanResult& scan) const {
    std::ostringstream prompt;
    prompt << "You are a research worker investigating a codebase area on behalf of "
           << "a larger research process. Investigate ONLY the code provided below.\n\n"
           << "Research goal: " << task.researchGoal << "\n"
           << "Success criteria: " << task.successCriteria << "\n\n"
           << "Code under investigation (area: " << task.targetArea << "):\n"
           << "-----\n"
           << scan.concatenatedContent << "\n"
           << "-----\n\n";

    if (!scan.filesSkippedBudget.empty()) {
        prompt << "Note: " << scan.filesSkippedBudget.size()
               << " file(s) in this area were omitted due to the context budget and were "
                  "NOT investigated.\n\n";
    }

    prompt << "Respond with ONLY a single JSON object, no prose, no markdown fences, "
              "matching this shape exactly:\n"
           << "{\n"
           << "  \"outcome\": \"success\" | \"no_information\" | \"partial\",\n"
           << "  \"summary\": \"compact findings relevant to the research goal, or empty "
              "string if none\",\n"
           << "  \"directions\": [\n"
           << "    {\n"
           << "      \"target_area\": \"relative path to investigate next\",\n"
           << "      \"research_goal\": \"what to look for there\",\n"
           << "      \"success_criteria\": \"how to know that goal is satisfied\"\n"
           << "    }\n"
           << "  ]\n"
           << "}\n\n"
           << "Use \"success\" only if the success criteria are fully met by what you can "
           << "see. Use \"no_information\" if this area is entirely unrelated to the goal. "
           << "Use \"partial\" otherwise, and populate \"directions\" with plausible leads "
           << "that stay on-topic for the research goal above -- do not suggest unrelated "
           << "areas. \"directions\" may be an empty array.";
    return prompt.str();
}

Finding Worker::ParseWorkerResponse(const std::string& raw, const std::string& area) {
    Finding finding;
    finding.areaInvestigated = area;

    std::string jsonStr = ExtractJsonObject(raw);
    if (jsonStr.empty()) {
        spdlog::warn("Worker: no JSON object found in model response for area '{}'", area);
        finding.outcome = WorkerOutcome::NoInformation;
        return finding;
    }

    try {
        json parsed = json::parse(jsonStr);
        std::string outcomeStr = parsed.value("outcome", "no_information");
        if (outcomeStr == "success") {
            finding.outcome = WorkerOutcome::Success;
        } else if (outcomeStr == "partial") {
            finding.outcome = WorkerOutcome::PartialWithDirections;
        } else {
            finding.outcome = WorkerOutcome::NoInformation;
        }
        finding.summary = parsed.value("summary", std::string{});

        for (const auto& dir : parsed.value("directions", json::array())) {
            Task t;
            t.id = "task-" + std::to_string(TaskCounter());
            t.targetArea = dir.value("target_area", std::string{});
            t.researchGoal = dir.value("research_goal", std::string{});
            t.successCriteria = dir.value("success_criteria", std::string{});
            if (!t.targetArea.empty() && !t.researchGoal.empty()) {
                finding.suggestedDirections.push_back(std::move(t));
            }
        }
    } catch (const json::exception& e) {
        spdlog::warn("Worker: failed to parse model JSON for area '{}': {}", area, e.what());
        finding.outcome = WorkerOutcome::NoInformation;
    }
    return finding;
}

Finding Worker::ExecuteSingleArea(const Task& task, const std::string& area) const {
    auto start = Clock::now();

    Task singleAreaTask = task;
    singleAreaTask.targetArea = area;

    ScanResult scan = scanner_.Scan(area, tokenBudget_);
    Finding finding;
    finding.areaInvestigated = area;
    finding.promptTokensApprox = scan.approxTokens;

    if (scan.filesIncluded.empty()) {
        finding.outcome = WorkerOutcome::NoInformation;
        finding.duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
        return finding;
    }

    std::string prompt = BuildPrompt(singleAreaTask, scan);
    spdlog::debug("Worker: investigating '{}' ({} files, ~{} tokens)", area,
                  scan.filesIncluded.size(), scan.approxTokens);
    auto response = client_.Generate(prompt);
    if (!response) {
        spdlog::error("Worker: Ollama call failed for area '{}'", area);
        finding.outcome = WorkerOutcome::NoInformation;
        finding.duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
        return finding;
    }

    finding = ParseWorkerResponse(*response, area);
    finding.promptTokensApprox = scan.approxTokens;
    finding.duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    return finding;
}

Finding Worker::Execute(const Task& task) const {
    if (!task.repeatable || task.repeatTargets.empty()) {
        return ExecuteSingleArea(task, task.targetArea);
    }

    // Repeatable task: apply the same goal across many sub-areas (e.g. a
    // pattern search across N functions/files) and merge the results.
    Finding merged;
    merged.areaInvestigated = task.targetArea;
    merged.outcome = WorkerOutcome::NoInformation;
    std::ostringstream summary;
    auto mergeStart = Clock::now();

    for (const auto& subArea : task.repeatTargets) {
        Finding sub = ExecuteSingleArea(task, subArea);
        merged.promptTokensApprox += sub.promptTokensApprox;

        if (sub.outcome == WorkerOutcome::Success ||
            sub.outcome == WorkerOutcome::PartialWithDirections) {
            if (!sub.summary.empty()) {
                summary << "[" << subArea << "] " << sub.summary << "\n";
            }
            for (auto& dir : sub.suggestedDirections) {
                merged.suggestedDirections.push_back(std::move(dir));
            }
        }
        if (sub.outcome == WorkerOutcome::Success &&
            merged.outcome != WorkerOutcome::Success) {
            merged.outcome = WorkerOutcome::Success;
        } else if (sub.outcome == WorkerOutcome::PartialWithDirections &&
                   merged.outcome == WorkerOutcome::NoInformation) {
            merged.outcome = WorkerOutcome::PartialWithDirections;
        }
    }

    merged.summary = summary.str();
    merged.duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - mergeStart);
    return merged;
}

}  // namespace cppcoder
