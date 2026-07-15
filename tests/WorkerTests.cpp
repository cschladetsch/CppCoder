#include "cppcoder/Worker.h"

#include <gtest/gtest.h>

using cppcoder::Finding;
using cppcoder::Worker;
using cppcoder::WorkerOutcome;

TEST(ParseWorkerResponseTest, SuccessOutcomeParsed) {
    std::string raw = R"({"outcome":"success","summary":"found it","directions":[]})";
    Finding f = Worker::ParseWorkerResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, WorkerOutcome::Success);
    EXPECT_EQ(f.summary, "found it");
    EXPECT_TRUE(f.suggestedDirections.empty());
    EXPECT_EQ(f.areaInvestigated, "src/foo.cpp");
}

TEST(ParseWorkerResponseTest, PartialOutcomeWithDirectionsParsed) {
    std::string raw = R"({
        "outcome":"partial",
        "summary":"some info",
        "directions":[
            {"target_area":"src/bar.cpp","research_goal":"find bar","success_criteria":"bar found"}
        ]
    })";
    Finding f = Worker::ParseWorkerResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, WorkerOutcome::PartialWithDirections);
    ASSERT_EQ(f.suggestedDirections.size(), 1u);
    EXPECT_EQ(f.suggestedDirections[0].targetArea, "src/bar.cpp");
    EXPECT_EQ(f.suggestedDirections[0].researchGoal, "find bar");
    EXPECT_EQ(f.suggestedDirections[0].successCriteria, "bar found");
}

TEST(ParseWorkerResponseTest, NoInformationOutcomeParsed) {
    std::string raw = R"({"outcome":"no_information","summary":"","directions":[]})";
    Finding f = Worker::ParseWorkerResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, WorkerOutcome::NoInformation);
}

TEST(ParseWorkerResponseTest, UnknownOutcomeStringDefaultsToNoInformation) {
    std::string raw = R"({"outcome":"maybe","summary":""})";
    Finding f = Worker::ParseWorkerResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, WorkerOutcome::NoInformation);
}

TEST(ParseWorkerResponseTest, ResponseWrappedInProseAndMarkdownFences) {
    std::string raw = "Sure, here's my analysis:\n```json\n"
                       R"({"outcome":"success","summary":"ok","directions":[]})"
                       "\n```\nHope that helps!";
    Finding f = Worker::ParseWorkerResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, WorkerOutcome::Success);
    EXPECT_EQ(f.summary, "ok");
}

TEST(ParseWorkerResponseTest, MissingDirectionsFieldDefaultsToEmpty) {
    std::string raw = R"({"outcome":"partial","summary":"x"})";
    Finding f = Worker::ParseWorkerResponse(raw, "area");
    EXPECT_TRUE(f.suggestedDirections.empty());
}

TEST(ParseWorkerResponseTest, DirectionsMissingTargetAreaAreFiltered) {
    std::string raw = R"({
        "outcome":"partial",
        "directions":[
            {"research_goal":"no area given"},
            {"target_area":"src/valid.cpp","research_goal":"valid one"}
        ]
    })";
    Finding f = Worker::ParseWorkerResponse(raw, "area");
    ASSERT_EQ(f.suggestedDirections.size(), 1u);
    EXPECT_EQ(f.suggestedDirections[0].targetArea, "src/valid.cpp");
}

TEST(ParseWorkerResponseTest, DirectionsMissingResearchGoalAreFiltered) {
    std::string raw = R"({
        "outcome":"partial",
        "directions":[
            {"target_area":"src/no_goal.cpp"}
        ]
    })";
    Finding f = Worker::ParseWorkerResponse(raw, "area");
    EXPECT_TRUE(f.suggestedDirections.empty());
}

TEST(ParseWorkerResponseTest, MalformedJsonFallsBackToNoInformation) {
    std::string raw = R"({"outcome": "success", "summary": )";  // truncated/invalid
    Finding f = Worker::ParseWorkerResponse(raw, "area");
    EXPECT_EQ(f.outcome, WorkerOutcome::NoInformation);
}

TEST(ParseWorkerResponseTest, EmptyRawStringFallsBackToNoInformation) {
    Finding f = Worker::ParseWorkerResponse("", "area");
    EXPECT_EQ(f.outcome, WorkerOutcome::NoInformation);
}

TEST(ParseWorkerResponseTest, NoJsonAtAllFallsBackToNoInformation) {
    Finding f = Worker::ParseWorkerResponse("I couldn't find any JSON to give you.", "area");
    EXPECT_EQ(f.outcome, WorkerOutcome::NoInformation);
}

TEST(ParseWorkerResponseTest, MultipleDirectionsAllParsedWhenValid) {
    std::string raw = R"({
        "outcome":"partial",
        "directions":[
            {"target_area":"a.cpp","research_goal":"g1","success_criteria":"c1"},
            {"target_area":"b.cpp","research_goal":"g2","success_criteria":"c2"},
            {"target_area":"c.cpp","research_goal":"g3","success_criteria":"c3"}
        ]
    })";
    Finding f = Worker::ParseWorkerResponse(raw, "area");
    EXPECT_EQ(f.suggestedDirections.size(), 3u);
}

TEST(ParseWorkerResponseTest, GeneratedDirectionIdsAreNonEmptyAndUnique) {
    std::string raw = R"({
        "outcome":"partial",
        "directions":[
            {"target_area":"a.cpp","research_goal":"g1"},
            {"target_area":"b.cpp","research_goal":"g2"}
        ]
    })";
    Finding f = Worker::ParseWorkerResponse(raw, "area");
    ASSERT_EQ(f.suggestedDirections.size(), 2u);
    EXPECT_FALSE(f.suggestedDirections[0].id.empty());
    EXPECT_FALSE(f.suggestedDirections[1].id.empty());
    EXPECT_NE(f.suggestedDirections[0].id, f.suggestedDirections[1].id);
}
