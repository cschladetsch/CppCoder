#pragma once

#include "cppcoder/OllamaClient.h"
#include "cppcoder/Types.h"

namespace cppcoder {

// Reviews a worker's Finding against the main research topic. Its only
// job is pruning: discard summary content that isn't actually related
// to the topic, and drop suggested directions that drift off-topic, so
// the task queue doesn't wander into unrelated areas.
class Judge {
public:
    explicit Judge(const OllamaClient& client);

    Finding Review(const std::string& mainResearchTopic, Finding finding) const;

    // Pure, side-effect-free application of a judge model response onto
    // an existing Finding. Public and static so it can be unit tested
    // directly without a running Ollama instance.
    static Finding ApplyJudgeResponse(const std::string& raw, Finding finding);

private:
    const OllamaClient& client_;

    std::string BuildPrompt(const std::string& topic, const Finding& finding) const;
};

}  // namespace cppcoder
