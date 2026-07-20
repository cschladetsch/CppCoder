#include "cppcoder/Editor.h"

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

// CodebaseScanner prepends "\n// ==== <path> ====\n" as a synthetic
// separator before each file's content when building the prompt (see
// CodebaseScanner.cpp). Despite being told this isn't real file content
// (BuildPrompt below), small local models sometimes echo it back
// verbatim in "new_content" anyway -- strip it defensively rather than
// trust the instruction alone, same don't-trust-model-output philosophy
// as JsonUtil's prose/fence stripping.
std::string StripScannerHeaderMarker(std::string content) {
    std::size_t pos = 0;
    while (pos < content.size() && content[pos] == '\n') ++pos;
    if (content.compare(pos, 8, "// ==== ") == 0) {
        std::size_t lineEnd = content.find('\n', pos);
        if (lineEnd != std::string::npos) {
            return content.substr(lineEnd + 1);
        }
    }
    return content;
}

}  // namespace

Editor::Editor(const OllamaClient& client, const CodebaseScanner& scanner,
               std::size_t tokenBudget)
    : client_(client), scanner_(scanner), tokenBudget_(tokenBudget) {}

std::string Editor::BuildPrompt(const Task& task, const ScanResult& scan) const {
    std::ostringstream prompt;
    prompt << "You are a coding agent making a change to a codebase on behalf of a larger "
           << "edit process. Change ONLY the code provided below.\n\n"
           << "Task: " << task.researchGoal << "\n"
           << "Success criteria: " << task.successCriteria << "\n\n"
           << "Code under consideration (area: " << task.targetArea << "):\n"
           << "-----\n"
           << scan.concatenatedContent << "\n"
           << "-----\n\n";

    if (!scan.filesSkippedBudget.empty()) {
        prompt << "Note: " << scan.filesSkippedBudget.size()
               << " file(s) in this area were omitted due to the context budget and were "
                  "NOT shown to you -- do not propose edits to files you have not seen.\n\n";
    }

    prompt << "Each file above is preceded by a \"// ==== <path> ====\" line. That marker is "
           << "inserted by the tool showing you this code -- it is NOT part of any file's "
           << "actual content. Never include it in \"new_content\".\n\n";

    prompt << "Respond with ONLY a single JSON object, no prose, no markdown fences, "
              "matching this shape exactly:\n"
           << "{\n"
           << "  \"outcome\": \"success\" | \"no_change\" | \"partial\",\n"
           << "  \"summary\": \"compact description of what changed and why, or empty "
              "string if nothing changed\",\n"
           << "  \"edits\": [\n"
           << "    {\n"
           << "      \"path\": \"relative path of the file to change\",\n"
           << "      \"new_content\": \"the FULL new content of the file after the "
              "change, not just the changed lines\",\n"
           << "      \"description\": \"short summary of the change to this file\"\n"
           << "    }\n"
           << "  ],\n"
           << "  \"directions\": [\n"
           << "    {\n"
           << "      \"target_area\": \"relative path that also needs a change\",\n"
           << "      \"research_goal\": \"what change to make there\",\n"
           << "      \"success_criteria\": \"how to know that change is complete\"\n"
           << "    }\n"
           << "  ]\n"
           << "}\n\n"
           << "Use \"success\" only if the success criteria are fully met by the edits you "
           << "propose. Use \"no_change\" if nothing in this area needs to change. Use "
           << "\"partial\" if you made edits here but other areas also need changes to fully "
           << "satisfy the task, and populate \"directions\" with those areas -- do not "
           << "suggest unrelated areas. Every entry in \"edits\" must include the complete "
           << "file content, not a diff or excerpt. \"edits\" and \"directions\" may be "
           << "empty arrays.";
    return prompt.str();
}

EditFinding Editor::ParseEditResponse(const std::string& raw, const std::string& area) {
    EditFinding finding;
    finding.areaInvestigated = area;

    std::string jsonStr = ExtractJsonObject(raw);
    if (jsonStr.empty()) {
        spdlog::warn("Editor: no JSON object found in model response for area '{}'", area);
        finding.outcome = EditOutcome::NoChangeNeeded;
        return finding;
    }

    try {
        json parsed = json::parse(jsonStr);
        std::string outcomeStr = parsed.value("outcome", "no_change");
        if (outcomeStr == "success") {
            finding.outcome = EditOutcome::Success;
        } else if (outcomeStr == "partial") {
            finding.outcome = EditOutcome::PartialWithDirections;
        } else {
            finding.outcome = EditOutcome::NoChangeNeeded;
        }
        finding.summary = parsed.value("summary", std::string{});

        for (const auto& e : parsed.value("edits", json::array())) {
            ProposedEdit edit;
            edit.path = e.value("path", std::string{});
            edit.newContent = StripScannerHeaderMarker(e.value("new_content", std::string{}));
            edit.description = e.value("description", std::string{});
            if (!edit.path.empty()) {
                finding.edits.push_back(std::move(edit));
            }
        }

        for (const auto& dir : parsed.value("directions", json::array())) {
            Task t;
            t.id = "edit-task-" + std::to_string(TaskCounter());
            t.targetArea = dir.value("target_area", std::string{});
            t.researchGoal = dir.value("research_goal", std::string{});
            t.successCriteria = dir.value("success_criteria", std::string{});
            if (!t.targetArea.empty() && !t.researchGoal.empty()) {
                finding.suggestedDirections.push_back(std::move(t));
            }
        }
    } catch (const json::exception& e) {
        spdlog::warn("Editor: failed to parse model JSON for area '{}': {}", area, e.what());
        finding.outcome = EditOutcome::NoChangeNeeded;
    }
    return finding;
}

EditFinding Editor::ExecuteSingleArea(const Task& task, const std::string& area) const {
    auto start = Clock::now();

    Task singleAreaTask = task;
    singleAreaTask.targetArea = area;

    ScanResult scan = scanner_.Scan(area, tokenBudget_);
    EditFinding finding;
    finding.areaInvestigated = area;
    finding.promptTokensApprox = scan.approxTokens;

    if (scan.filesIncluded.empty()) {
        finding.outcome = EditOutcome::NoChangeNeeded;
        finding.duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
        return finding;
    }

    std::string prompt = BuildPrompt(singleAreaTask, scan);
    spdlog::debug("Editor: considering '{}' ({} files, ~{} tokens)", area,
                  scan.filesIncluded.size(), scan.approxTokens);
    auto response = client_.Generate(prompt);
    if (!response) {
        spdlog::error("Editor: Ollama call failed for area '{}'", area);
        finding.outcome = EditOutcome::NoChangeNeeded;
        finding.duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
        return finding;
    }

    finding = ParseEditResponse(*response, area);
    finding.promptTokensApprox = scan.approxTokens;
    finding.duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start);
    return finding;
}

EditFinding Editor::Execute(const Task& task) const {
    if (!task.repeatable || task.repeatTargets.empty()) {
        return ExecuteSingleArea(task, task.targetArea);
    }

    // Repeatable task: apply the same goal across many sub-areas (e.g. a
    // keyword-seeded set of candidate files) and merge the results.
    EditFinding merged;
    merged.areaInvestigated = task.targetArea;
    merged.outcome = EditOutcome::NoChangeNeeded;
    std::ostringstream summary;
    auto mergeStart = Clock::now();

    for (const auto& subArea : task.repeatTargets) {
        EditFinding sub = ExecuteSingleArea(task, subArea);
        merged.promptTokensApprox += sub.promptTokensApprox;

        if (sub.outcome == EditOutcome::Success ||
            sub.outcome == EditOutcome::PartialWithDirections) {
            if (!sub.summary.empty()) {
                summary << "[" << subArea << "] " << sub.summary << "\n";
            }
            for (auto& edit : sub.edits) {
                merged.edits.push_back(std::move(edit));
            }
            for (auto& dir : sub.suggestedDirections) {
                merged.suggestedDirections.push_back(std::move(dir));
            }
        }
        if (sub.outcome == EditOutcome::Success && merged.outcome != EditOutcome::Success) {
            merged.outcome = EditOutcome::Success;
        } else if (sub.outcome == EditOutcome::PartialWithDirections &&
                   merged.outcome == EditOutcome::NoChangeNeeded) {
            merged.outcome = EditOutcome::PartialWithDirections;
        }
    }

    merged.summary = summary.str();
    merged.duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - mergeStart);
    return merged;
}

}  // namespace cppcoder
