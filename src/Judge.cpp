#include "cppcoder/Judge.h"

#include "cppcoder/JsonUtil.h"

#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>
#include <sstream>

namespace cppcoder {

using json = nlohmann::json;

Judge::Judge(const OllamaClient& client) : client_(client) {}

std::string Judge::BuildPrompt(const std::string& topic, const Finding& finding) const {
    std::ostringstream prompt;
    prompt << "You are the judge in a research pipeline. A worker investigated area '"
           << finding.areaInvestigated << "' and produced the findings below. Your only "
              "job is to prune, not to add new information.\n\n"
           << "Main research topic: " << topic << "\n\n"
           << "Worker summary:\n" << (finding.summary.empty() ? "(empty)" : finding.summary)
           << "\n\n"
           << "Candidate follow-up directions:\n";

    for (std::size_t i = 0; i < finding.suggestedDirections.size(); ++i) {
        const auto& d = finding.suggestedDirections[i];
        prompt << i << ". target_area=\"" << d.targetArea << "\" goal=\"" << d.researchGoal
               << "\"\n";
    }
    if (finding.suggestedDirections.empty()) {
        prompt << "(none)\n";
    }

    prompt << "\nRespond with ONLY a single JSON object, no prose, no markdown fences:\n"
           << "{\n"
           << "  \"summary_relevant\": true | false,\n"
           << "  \"filtered_summary\": \"the summary text, trimmed to only what is "
              "actually relevant to the main research topic (may equal the original, or "
              "be empty)\",\n"
           << "  \"keep_direction_indices\": [list of integer indices from the candidate "
              "list above that stay clearly aligned with the main research topic]\n"
           << "}";
    return prompt.str();
}

Finding Judge::ApplyJudgeResponse(const std::string& raw, Finding finding) {
    std::string jsonStr = ExtractJsonObject(raw);
    if (jsonStr.empty()) {
        spdlog::warn("Judge: no JSON object in response, passing finding through unfiltered");
        return finding;
    }

    try {
        json parsed = json::parse(jsonStr);
        bool relevant = parsed.value("summary_relevant", true);
        finding.summary = relevant ? parsed.value("filtered_summary", finding.summary)
                                    : std::string{};

        std::vector<Task> kept;
        for (const auto& idx : parsed.value("keep_direction_indices", json::array())) {
            if (!idx.is_number_unsigned()) continue;
            std::size_t i = idx.get<std::size_t>();
            if (i < finding.suggestedDirections.size()) {
                kept.push_back(finding.suggestedDirections[i]);
            }
        }
        finding.suggestedDirections = std::move(kept);

        if (finding.outcome == WorkerOutcome::PartialWithDirections && finding.summary.empty() &&
            finding.suggestedDirections.empty()) {
            // The judge found nothing salvageable in this finding.
            finding.outcome = WorkerOutcome::NoInformation;
        }
    } catch (const json::exception& e) {
        spdlog::warn("Judge: failed to parse response JSON: {}, passing finding through unfiltered",
                     e.what());
    }

    return finding;
}

Finding Judge::Review(const std::string& mainResearchTopic, Finding finding) const {
    if (finding.outcome == WorkerOutcome::NoInformation) {
        // Nothing to prune.
        return finding;
    }

    std::string prompt = BuildPrompt(mainResearchTopic, finding);
    auto response = client_.Generate(prompt);
    if (!response) {
        spdlog::error("Judge: Ollama call failed for area '{}', passing finding through unfiltered",
                      finding.areaInvestigated);
        return finding;
    }

    return ApplyJudgeResponse(*response, std::move(finding));
}

}  // namespace cppcoder
