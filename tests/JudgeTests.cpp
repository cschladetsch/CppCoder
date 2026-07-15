#include "cppcoder/Judge.h"

#include <gtest/gtest.h>

using cppcoder::Finding;
using cppcoder::Judge;
using cppcoder::Task;
using cppcoder::WorkerOutcome;

namespace {

Finding MakePartialFinding(std::vector<Task> directions, std::string summary = "some summary") {
    Finding f;
    f.outcome = WorkerOutcome::PartialWithDirections;
    f.summary = std::move(summary);
    f.suggestedDirections = std::move(directions);
    f.areaInvestigated = "src/foo.cpp";
    return f;
}

Task MakeDirection(const std::string& id, const std::string& area) {
    Task t;
    t.id = id;
    t.targetArea = area;
    t.researchGoal = "goal for " + area;
    return t;
}

}  // namespace

TEST(ApplyJudgeResponseTest, KeepsIndicatedDirectionsByIndex) {
    Finding f = MakePartialFinding({MakeDirection("d0", "a.cpp"), MakeDirection("d1", "b.cpp"),
                                     MakeDirection("d2", "c.cpp")});
    std::string raw = R"({"summary_relevant":true,"filtered_summary":"trimmed",
                           "keep_direction_indices":[0,2]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    ASSERT_EQ(result.suggestedDirections.size(), 2u);
    EXPECT_EQ(result.suggestedDirections[0].targetArea, "a.cpp");
    EXPECT_EQ(result.suggestedDirections[1].targetArea, "c.cpp");
    EXPECT_EQ(result.summary, "trimmed");
}

TEST(ApplyJudgeResponseTest, DropsAllDirectionsWhenIndicesEmpty) {
    Finding f = MakePartialFinding({MakeDirection("d0", "a.cpp")});
    std::string raw = R"({"summary_relevant":true,"filtered_summary":"kept",
                           "keep_direction_indices":[]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_TRUE(result.suggestedDirections.empty());
}

TEST(ApplyJudgeResponseTest, OutOfRangeIndicesAreIgnored) {
    Finding f = MakePartialFinding({MakeDirection("d0", "a.cpp")});
    std::string raw = R"({"summary_relevant":true,"filtered_summary":"x",
                           "keep_direction_indices":[0,5,99]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    ASSERT_EQ(result.suggestedDirections.size(), 1u);
    EXPECT_EQ(result.suggestedDirections[0].targetArea, "a.cpp");
}

TEST(ApplyJudgeResponseTest, SummaryRelevantFalseEmptiesSummary) {
    Finding f = MakePartialFinding({}, "original summary");
    std::string raw = R"({"summary_relevant":false,"filtered_summary":"should be ignored",
                           "keep_direction_indices":[]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_TRUE(result.summary.empty());
}

TEST(ApplyJudgeResponseTest, MalformedJsonPassesFindingThroughUnchanged) {
    Finding f = MakePartialFinding({MakeDirection("d0", "a.cpp")}, "unchanged summary");
    std::string raw = "not valid json at all, no braces";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_EQ(result.summary, "unchanged summary");
    ASSERT_EQ(result.suggestedDirections.size(), 1u);
    EXPECT_EQ(result.suggestedDirections[0].targetArea, "a.cpp");
}

TEST(ApplyJudgeResponseTest, TruncatedJsonPassesFindingThroughUnchanged) {
    Finding f = MakePartialFinding({}, "still here");
    std::string raw = R"({"summary_relevant": true, "filtered_summary": )";  // truncated
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_EQ(result.summary, "still here");
}

TEST(ApplyJudgeResponseTest, NonArrayKeepIndicesTreatedAsEmpty) {
    Finding f = MakePartialFinding({MakeDirection("d0", "a.cpp")});
    std::string raw = R"({"summary_relevant":true,"filtered_summary":"x",
                           "keep_direction_indices":"not an array"})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    // A non-array value for keep_direction_indices iterates as a single
    // non-numeric element, which is skipped, so nothing is kept -- this
    // should not throw or crash.
    EXPECT_TRUE(result.suggestedDirections.empty());
}

TEST(ApplyJudgeResponseTest, DowngradesToNoInformationWhenNothingSalvaged) {
    Finding f = MakePartialFinding({MakeDirection("d0", "a.cpp")}, "some summary");
    std::string raw = R"({"summary_relevant":false,"filtered_summary":"",
                           "keep_direction_indices":[]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_EQ(result.outcome, WorkerOutcome::NoInformation);
}

TEST(ApplyJudgeResponseTest, DoesNotDowngradeSuccessOutcome) {
    Finding f;
    f.outcome = WorkerOutcome::Success;
    f.summary = "answer found";
    std::string raw = R"({"summary_relevant":true,"filtered_summary":"answer found",
                           "keep_direction_indices":[]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_EQ(result.outcome, WorkerOutcome::Success);
}

TEST(ApplyJudgeResponseTest, KeepsPartialOutcomeWhenDirectionsSurvive) {
    Finding f = MakePartialFinding({MakeDirection("d0", "a.cpp")}, "");
    std::string raw = R"({"summary_relevant":true,"filtered_summary":"",
                           "keep_direction_indices":[0]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_EQ(result.outcome, WorkerOutcome::PartialWithDirections);
}

TEST(ApplyJudgeResponseTest, EmptyCandidateDirectionsHandledGracefully) {
    Finding f = MakePartialFinding({}, "summary text");
    std::string raw = R"({"summary_relevant":true,"filtered_summary":"summary text",
                           "keep_direction_indices":[]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_TRUE(result.suggestedDirections.empty());
    EXPECT_EQ(result.summary, "summary text");
}

TEST(ApplyJudgeResponseTest, MissingSummaryRelevantFieldDefaultsToTrue) {
    Finding f = MakePartialFinding({}, "original");
    std::string raw = R"({"filtered_summary":"new summary","keep_direction_indices":[]})";
    Finding result = Judge::ApplyJudgeResponse(raw, f);
    EXPECT_EQ(result.summary, "new summary");
}
