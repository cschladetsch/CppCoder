#include "cppcoder/Types.h"

#include <gtest/gtest.h>

using cppcoder::EstimateTokens;
using cppcoder::Task;
using cppcoder::WorkerOutcome;

TEST(EstimateTokensTest, EmptyStringIsNonZero) {
    // The +1 fudge factor means even empty input estimates to 1 token.
    EXPECT_EQ(EstimateTokens(""), 1u);
}

TEST(EstimateTokensTest, ScalesRoughlyWithLength) {
    std::string text(400, 'x');
    EXPECT_EQ(EstimateTokens(text), 101u);  // 400/4 + 1
}

TEST(EstimateTokensTest, ShortStringUnderOneCharPerFour) {
    EXPECT_EQ(EstimateTokens("abc"), 1u);  // 3/4 + 1 == 0 + 1
}

TEST(EstimateTokensTest, MonotonicWithLength) {
    EXPECT_LT(EstimateTokens("short"), EstimateTokens(std::string(1000, 'a')));
}

TEST(EstimateTokensTest, LargeBudgetTaskUnderTypicalContextWindow) {
    // Sanity check against the spec's empirical 100-150K token ceiling.
    std::string content(600'000, 'a');  // ~150K tokens
    EXPECT_LE(EstimateTokens(content), 150'001u);
}

TEST(TaskDefaultsTest, AggregateDefaultsAreSane) {
    Task t;
    EXPECT_TRUE(t.id.empty());
    EXPECT_TRUE(t.targetArea.empty());
    EXPECT_EQ(t.depth, 0);
    EXPECT_FALSE(t.repeatable);
    EXPECT_TRUE(t.repeatTargets.empty());
    EXPECT_TRUE(t.parentId.empty());
}

TEST(TaskDefaultsTest, AggregateInitPopulatesGivenFields) {
    Task t{"id1", "area/path", "find X", "X is found"};
    EXPECT_EQ(t.id, "id1");
    EXPECT_EQ(t.targetArea, "area/path");
    EXPECT_EQ(t.researchGoal, "find X");
    EXPECT_EQ(t.successCriteria, "X is found");
    EXPECT_EQ(t.depth, 0);
}

TEST(WorkerOutcomeTest, ValuesAreDistinct) {
    EXPECT_NE(WorkerOutcome::Success, WorkerOutcome::NoInformation);
    EXPECT_NE(WorkerOutcome::Success, WorkerOutcome::PartialWithDirections);
    EXPECT_NE(WorkerOutcome::PartialWithDirections, WorkerOutcome::NoInformation);
}
