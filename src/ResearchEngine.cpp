#include "cppcoder/ResearchEngine.h"

#include "cppcoder/JsonUtil.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace cppcoder {

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

std::vector<std::string> FallbackKeywords(const std::string& question, std::size_t maxCount) {
    static const std::unordered_set<std::string> stopwords = {
        "the", "a",   "an",  "is",   "are", "was",  "were", "does", "do",  "did",
        "how", "what","why", "when", "where","which","who",  "this", "that","for",
        "and", "or",  "to",  "of",   "in",  "on",   "with",  "it",   "can", "i",
    };
    std::vector<std::string> words;
    std::string current;
    for (char c : question) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            current += c;
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) words.push_back(current);

    std::vector<std::string> keywords;
    for (auto& w : words) {
        std::string lower = w;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.size() < 3 || stopwords.count(lower)) continue;
        keywords.push_back(w);
        if (keywords.size() >= maxCount) break;
    }
    return keywords;
}

namespace {

std::string OutcomeToString(WorkerOutcome outcome) {
    switch (outcome) {
        case WorkerOutcome::Success:
            return "success";
        case WorkerOutcome::PartialWithDirections:
            return "partial";
        case WorkerOutcome::NoInformation:
        default:
            return "no_information";
    }
}

json DirectionsToJson(const std::vector<Task>& directions) {
    json arr = json::array();
    for (const auto& d : directions) {
        arr.push_back({{"id", d.id}, {"target_area", d.targetArea}, {"research_goal", d.researchGoal}});
    }
    return arr;
}

}  // namespace

ResearchEngine::ResearchEngine(OllamaClient client, CodebaseScanner scanner, EngineConfig config)
    : client_(std::move(client)),
      scanner_(std::move(scanner)),
      config_(config),
      worker_(client_, scanner_, config.tokenBudgetPerTask),
      judge_(client_) {}

void ResearchEngine::Emit(const std::string& jsonLine) const {
    if (eventSink_) eventSink_(jsonLine);
}

std::vector<std::string> ResearchEngine::ExtractKeywords(const std::string& question) const {
    std::ostringstream prompt;
    prompt << "Extract up to " << config_.maxInitialKeywords
           << " short keywords or identifier-like terms from this question that would "
              "help locate relevant code in a codebase (function names, type names, "
              "file names, domain terms). Respond with ONLY a JSON array of strings, no "
              "prose, no markdown fences.\n\nQuestion: "
           << question;

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
                spdlog::warn("ResearchEngine: keyword JSON parse failed: {}", e.what());
            }
        }
    }

    spdlog::info("ResearchEngine: falling back to naive keyword extraction");
    return FallbackKeywords(question, config_.maxInitialKeywords);
}

std::vector<Task> ResearchEngine::SeedInitialTasks(const std::string& question,
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
    seed.id = "seed";
    seed.targetArea = "(keyword probe: " + std::to_string(candidateAreas.size()) + " files)";
    seed.researchGoal = question;
    seed.successCriteria =
        "Found information in the code that directly and fully answers the question above.";
    seed.repeatable = true;
    seed.repeatTargets = candidateAreas;
    return {seed};
}

std::string ResearchEngine::SynthesizeAnswer(const std::string& question,
                                              const std::vector<Finding>& successfulFindings) const {
    std::ostringstream prompt;
    prompt << "You are synthesizing a final answer from research findings gathered across "
              "a codebase.\n\nOriginal question: "
           << question << "\n\nFindings:\n";
    for (const auto& f : successfulFindings) {
        prompt << "- [" << f.areaInvestigated << "] " << f.summary << "\n";
    }
    prompt << "\nWrite a clear, direct answer to the original question using only the "
              "findings above. Plain text, no JSON, no markdown fences.";

    auto response = client_.Generate(prompt.str());
    if (response) return *response;

    std::ostringstream fallback;
    fallback << "Synthesis call failed; raw findings:\n";
    for (const auto& f : successfulFindings) {
        fallback << "- [" << f.areaInvestigated << "] " << f.summary << "\n";
    }
    return fallback.str();
}

ResearchResult ResearchEngine::Research(const std::string& question) {
    auto engineStart = Clock::now();
    ResearchResult result;
    TaskQueue queue;

    Emit(json{{"event", "question"}, {"question", question}}.dump());

    auto keywords = ExtractKeywords(question);
    Emit(json{{"event", "keywords_extracted"}, {"keywords", keywords}}.dump());

    auto seedTasks = SeedInitialTasks(question, keywords);
    if (seedTasks.empty()) {
        result.terminationReason =
            "no initial research direction found from question keywords "
            "(question keywords not present in codebase knowledge)";
        result.wallClock =
            std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - engineStart);
        Emit(json{{"event", "complete"},
                  {"answered", false},
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

    std::vector<Finding> successfulFindings;
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

        Finding rawFinding = worker_.Execute(task);
        Emit(json{{"event", "worker_result"},
                  {"id", task.id},
                  {"outcome", OutcomeToString(rawFinding.outcome)},
                  {"summary", rawFinding.summary},
                  {"directions", DirectionsToJson(rawFinding.suggestedDirections)},
                  {"duration_ms", rawFinding.duration.count()}}
                 .dump());

        Finding finding = judge_.Review(question, rawFinding);
        Emit(json{{"event", "judge_result"},
                  {"id", task.id},
                  {"outcome", OutcomeToString(finding.outcome)},
                  {"summary", finding.summary},
                  {"kept_directions", DirectionsToJson(finding.suggestedDirections)},
                  {"rejected_count", rawFinding.suggestedDirections.size() -
                                          finding.suggestedDirections.size()}}
                 .dump());

        queue.MarkVisited(task.targetArea);
        ++iterations;

        spdlog::info("[iter {}] area='{}' outcome={} directions={}", iterations,
                     task.targetArea, OutcomeToString(finding.outcome),
                     finding.suggestedDirections.size());

        if (finding.outcome == WorkerOutcome::Success) {
            successfulFindings.push_back(finding);
            Emit(json{{"event", "answer_progress"},
                      {"id", task.id},
                      {"summary", finding.summary}}
                     .dump());
        } else if (finding.outcome == WorkerOutcome::PartialWithDirections) {
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
        // NoInformation: dropped, area already marked visited.
    }

    result.iterationsRun = iterations;
    result.successfulFindings = successfulFindings;
    result.wallClock =
        std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - engineStart);

    if (!successfulFindings.empty()) {
        result.answered = true;
        result.answer = SynthesizeAnswer(question, successfulFindings);
        if (result.terminationReason.empty()) result.terminationReason = "task queue drained";
        Emit(json{{"event", "answer_found"}, {"answer", result.answer}}.dump());
    } else if (result.terminationReason.empty()) {
        result.terminationReason = "task queue drained without a successful finding";
    }

    Emit(json{{"event", "complete"},
              {"answered", result.answered},
              {"reason", result.terminationReason},
              {"iterations", result.iterationsRun},
              {"wall_clock_ms", result.wallClock.count()}}
             .dump());

    return result;
}

}  // namespace cppcoder
