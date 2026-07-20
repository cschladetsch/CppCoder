#include "cppcoder/EditEngine.h"

#include "cppcoder/JsonUtil.h"
#include "cppcoder/ResearchEngine.h"  // FallbackKeywords

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <sstream>

namespace cppcoder {

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

namespace {

std::string OutcomeToString(EditOutcome outcome) {
    switch (outcome) {
        case EditOutcome::Success:
            return "success";
        case EditOutcome::PartialWithDirections:
            return "partial";
        case EditOutcome::NoChangeNeeded:
        default:
            return "no_change";
    }
}

json DirectionsToJson(const std::vector<Task>& directions) {
    json arr = json::array();
    for (const auto& d : directions) {
        arr.push_back({{"id", d.id}, {"target_area", d.targetArea}, {"research_goal", d.researchGoal}});
    }
    return arr;
}

json EditsToJson(const std::vector<ProposedEdit>& edits) {
    json arr = json::array();
    for (const auto& e : edits) {
        arr.push_back({{"path", e.path}, {"description", e.description}});
    }
    return arr;
}

}  // namespace

EditEngine::EditEngine(OllamaClient client, CodebaseScanner scanner, std::filesystem::path root,
                        EditEngineConfig config)
    : client_(std::move(client)),
      scanner_(std::move(scanner)),
      root_(std::move(root)),
      config_(config),
      editor_(client_, scanner_, config.tokenBudgetPerTask),
      applier_(root_) {}

void EditEngine::Emit(const std::string& jsonLine) const {
    if (eventSink_) eventSink_(jsonLine);
}

std::vector<std::string> EditEngine::ExtractKeywords(const std::string& taskDescription) const {
    std::ostringstream prompt;
    prompt << "Extract up to " << config_.maxInitialKeywords
           << " short keywords or identifier-like terms from this coding task that would "
              "help locate the relevant code in a codebase (function names, type names, "
              "file names, domain terms). Respond with ONLY a JSON array of strings, no "
              "prose, no markdown fences.\n\nTask: "
           << taskDescription;

    auto response = client_.Generate(prompt.str());
    if (response) {
        std::string arrStr = ExtractJsonArray(*response);
        if (!arrStr.empty()) {
            try {
                json parsed = json::parse(arrStr);
                std::vector<std::string> keywords;
                for (const auto& k : parsed) {
                    if (k.is_string()) keywords.push_back(k.get<std::string>());
                    if (keywords.size() >= config_.maxInitialKeywords) break;
                }
                if (!keywords.empty()) return keywords;
            } catch (const json::exception& e) {
                spdlog::warn("EditEngine: keyword JSON parse failed: {}", e.what());
            }
        }
    }

    spdlog::info("EditEngine: falling back to naive keyword extraction");
    return FallbackKeywords(taskDescription, config_.maxInitialKeywords);
}

std::vector<Task> EditEngine::SeedInitialTasks(const std::string& taskDescription,
                                                const std::vector<std::string>& keywords) const {
    std::vector<std::string> candidateAreas;
    for (const auto& kw : keywords) {
        auto matches = scanner_.FindFilesMatchingKeyword(kw, 10);
        for (auto& m : matches) candidateAreas.push_back(std::move(m));
    }
    std::sort(candidateAreas.begin(), candidateAreas.end());
    candidateAreas.erase(std::unique(candidateAreas.begin(), candidateAreas.end()),
                          candidateAreas.end());
    if (candidateAreas.size() > 25) candidateAreas.resize(25);

    if (candidateAreas.empty()) {
        return {};
    }

    Task seed;
    seed.id = "edit-seed";
    seed.targetArea = "(keyword probe: " + std::to_string(candidateAreas.size()) + " files)";
    seed.researchGoal = taskDescription;
    seed.successCriteria =
        "Made the change described in the task, fully and correctly, across every file "
        "that needed it.";
    seed.repeatable = true;
    seed.repeatTargets = candidateAreas;
    return {seed};
}

EditRunResult EditEngine::Run(const std::string& taskDescription) {
    auto engineStart = Clock::now();
    EditRunResult result;
    TaskQueue queue;

    Emit(json{{"event", "task"}, {"task", taskDescription}}.dump());

    auto keywords = ExtractKeywords(taskDescription);
    Emit(json{{"event", "keywords_extracted"}, {"keywords", keywords}}.dump());

    auto seedTasks = SeedInitialTasks(taskDescription, keywords);
    if (seedTasks.empty()) {
        result.terminationReason =
            "no initial edit target found from task keywords "
            "(task keywords not present in codebase)";
        result.wallClock =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - engineStart);
        Emit(json{{"event", "complete"},
                  {"applied", false},
                  {"reason", result.terminationReason},
                  {"iterations", 0},
                  {"wall_clock_ms", result.wallClock.count()}}
                 .dump());
        return result;
    }
    for (const auto& t : seedTasks) {
        queue.Push(t);
        Emit(json{{"event", "task_queued"},
                  {"id", t.id},
                  {"parent_id", t.parentId},
                  {"target_area", t.targetArea},
                  {"research_goal", t.researchGoal},
                  {"depth", t.depth},
                  {"repeat_count", t.repeatTargets.size()}}
                 .dump());
    }

    std::vector<ProposedEdit> proposedEdits;
    ApplyOutcome applyOutcome;
    int iterations = 0;

    while (!queue.Empty()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(Clock::now() - engineStart);
        if (iterations >= config_.maxIterations) {
            result.terminationReason = "max iterations reached";
            break;
        }
        if (elapsed >= config_.maxWallClock) {
            result.terminationReason = "wall clock budget exhausted";
            break;
        }

        Task task = queue.Pop();
        Emit(json{{"event", "task_started"},
                  {"id", task.id},
                  {"target_area", task.targetArea},
                  {"depth", task.depth}}
                 .dump());

        EditFinding finding = editor_.Execute(task);
        Emit(json{{"event", "edit_result"},
                  {"id", task.id},
                  {"outcome", OutcomeToString(finding.outcome)},
                  {"summary", finding.summary},
                  {"edits", EditsToJson(finding.edits)},
                  {"directions", DirectionsToJson(finding.suggestedDirections)},
                  {"duration_ms", finding.duration.count()}}
                 .dump());

        queue.MarkVisited(task.targetArea);
        ++iterations;

        spdlog::info("[iter {}] area='{}' outcome={} edits={} directions={}", iterations,
                     task.targetArea, OutcomeToString(finding.outcome), finding.edits.size(),
                     finding.suggestedDirections.size());

        if ((finding.outcome == EditOutcome::Success ||
             finding.outcome == EditOutcome::PartialWithDirections) &&
            !finding.edits.empty()) {
            if (config_.apply) {
                ApplyOutcome applied = applier_.Apply(finding.edits);
                for (auto& p : applied.writtenPaths) {
                    Emit(json{{"event", "edit_applied"}, {"id", task.id}, {"path", p}}.dump());
                    applyOutcome.writtenPaths.push_back(p);
                }
                for (auto& p : applied.rejectedPaths) {
                    Emit(json{{"event", "edit_rejected"},
                              {"id", task.id},
                              {"path", p},
                              {"reason", "path escapes codebase root"}}
                             .dump());
                    applyOutcome.rejectedPaths.push_back(p);
                }
                for (auto& e : applied.errors) {
                    Emit(json{{"event", "edit_rejected"},
                              {"id", task.id},
                              {"path", e},
                              {"reason", "write error"}}
                             .dump());
                    applyOutcome.errors.push_back(e);
                }
            } else {
                for (auto& e : finding.edits) {
                    Emit(json{{"event", "edit_proposed"},
                              {"id", task.id},
                              {"path", e.path},
                              {"description", e.description}}
                             .dump());
                    proposedEdits.push_back(e);
                }
            }
        }

        if (finding.outcome == EditOutcome::PartialWithDirections) {
            for (auto dir : finding.suggestedDirections) {
                dir.depth = task.depth + 1;
                dir.parentId = task.id;
                if (queue.Push(dir)) {
                    Emit(json{{"event", "task_queued"},
                              {"id", dir.id},
                              {"parent_id", dir.parentId},
                              {"target_area", dir.targetArea},
                              {"research_goal", dir.researchGoal},
                              {"depth", dir.depth},
                              {"repeat_count", dir.repeatTargets.size()}}
                             .dump());
                }
            }
        }
        // NoChangeNeeded: dropped, area already marked visited.
    }

    result.iterationsRun = iterations;
    result.proposedEdits = proposedEdits;
    result.applyOutcome = applyOutcome;
    result.anyEdits = !proposedEdits.empty() || !applyOutcome.writtenPaths.empty();
    result.wallClock =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - engineStart);
    if (result.terminationReason.empty()) result.terminationReason = "task queue drained";

    Emit(json{{"event", "complete"},
              {"applied", result.anyEdits},
              {"reason", result.terminationReason},
              {"iterations", result.iterationsRun},
              {"wall_clock_ms", result.wallClock.count()}}
             .dump());

    return result;
}

}  // namespace cppcoder
