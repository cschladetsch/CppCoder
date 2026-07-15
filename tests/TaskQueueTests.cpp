#include "cppcoder/TaskQueue.h"

#include <gtest/gtest.h>

using cppcoder::Task;
using cppcoder::TaskQueue;

TEST(TaskQueueTest, PushAndPopPreservesOrder) {
    TaskQueue q;
    Task a{"a", "areaA", "goal", "criteria"};
    Task b{"b", "areaB", "goal", "criteria"};
    EXPECT_TRUE(q.Push(a));
    EXPECT_TRUE(q.Push(b));
    EXPECT_EQ(q.Size(), 2u);

    Task popped = q.Pop();
    EXPECT_EQ(popped.targetArea, "areaA");
    EXPECT_EQ(q.Size(), 1u);
}

TEST(TaskQueueTest, RejectsDuplicateQueuedArea) {
    TaskQueue q;
    Task a{"a", "areaA", "goal1", "criteria1"};
    Task dup{"dup", "areaA", "goal2", "criteria2"};
    EXPECT_TRUE(q.Push(a));
    EXPECT_FALSE(q.Push(dup));
    EXPECT_EQ(q.Size(), 1u);
}

TEST(TaskQueueTest, RejectsVisitedArea) {
    TaskQueue q;
    q.MarkVisited("areaA");
    Task a{"a", "areaA", "goal", "criteria"};
    EXPECT_FALSE(q.Push(a));
    EXPECT_TRUE(q.Empty());
}

TEST(TaskQueueTest, AllowsRequeueAfterPop) {
    TaskQueue q;
    Task a{"a", "areaA", "goal", "criteria"};
    EXPECT_TRUE(q.Push(a));
    q.Pop();
    // Not marked visited yet -- area should be re-queueable.
    EXPECT_TRUE(q.Push(a));
}

TEST(TaskQueueTest, EmptyQueueReportsEmptyAndZeroSize) {
    TaskQueue q;
    EXPECT_TRUE(q.Empty());
    EXPECT_EQ(q.Size(), 0u);
}

TEST(TaskQueueTest, PopDecreasesSizeAndClearsEmptyOnLastItem) {
    TaskQueue q;
    q.Push(Task{"a", "areaA", "g", "c"});
    EXPECT_FALSE(q.Empty());
    q.Pop();
    EXPECT_TRUE(q.Empty());
}

TEST(TaskQueueTest, FifoOrderHoldsForThreePlusItems) {
    TaskQueue q;
    q.Push(Task{"a", "1", "g", "c"});
    q.Push(Task{"b", "2", "g", "c"});
    q.Push(Task{"c", "3", "g", "c"});
    EXPECT_EQ(q.Pop().targetArea, "1");
    EXPECT_EQ(q.Pop().targetArea, "2");
    EXPECT_EQ(q.Pop().targetArea, "3");
    EXPECT_TRUE(q.Empty());
}

TEST(TaskQueueTest, VisitedQueryReflectsMarkVisited) {
    TaskQueue q;
    EXPECT_FALSE(q.Visited("areaA"));
    q.MarkVisited("areaA");
    EXPECT_TRUE(q.Visited("areaA"));
    EXPECT_FALSE(q.Visited("areaB"));
}

TEST(TaskQueueTest, MarkVisitedDoesNotAffectQueuedButUnvisitedArea) {
    TaskQueue q;
    q.Push(Task{"a", "areaA", "g", "c"});
    // Area is queued but not yet visited -- Visited() should say so.
    EXPECT_FALSE(q.Visited("areaA"));
}

TEST(TaskQueueTest, RepeatableTaskDedupesOnUmbrellaArea) {
    TaskQueue q;
    Task rep;
    rep.id = "seed";
    rep.targetArea = "(probe: 3 files)";
    rep.repeatable = true;
    rep.repeatTargets = {"a.cpp", "b.cpp", "c.cpp"};
    EXPECT_TRUE(q.Push(rep));

    Task repDup = rep;
    repDup.id = "seed2";
    EXPECT_FALSE(q.Push(repDup));
    EXPECT_EQ(q.Size(), 1u);
}

TEST(TaskQueueTest, RepeatableTaskRejectedIfUmbrellaAreaVisited) {
    TaskQueue q;
    q.MarkVisited("(probe: 3 files)");
    Task rep;
    rep.id = "seed";
    rep.targetArea = "(probe: 3 files)";
    rep.repeatable = true;
    rep.repeatTargets = {"a.cpp"};
    EXPECT_FALSE(q.Push(rep));
}

TEST(TaskQueueTest, DifferentAreasCanCoexist) {
    TaskQueue q;
    EXPECT_TRUE(q.Push(Task{"a", "areaA", "g", "c"}));
    EXPECT_TRUE(q.Push(Task{"b", "areaB", "g", "c"}));
    EXPECT_TRUE(q.Push(Task{"c", "areaC", "g", "c"}));
    EXPECT_EQ(q.Size(), 3u);
}

TEST(TaskQueueTest, PoppedTaskRetainsAllOriginalFields) {
    TaskQueue q;
    Task t{"id-1", "areaX", "find the thing", "thing is found"};
    t.depth = 3;
    t.parentId = "parent-1";
    q.Push(t);
    Task popped = q.Pop();
    EXPECT_EQ(popped.id, "id-1");
    EXPECT_EQ(popped.researchGoal, "find the thing");
    EXPECT_EQ(popped.successCriteria, "thing is found");
    EXPECT_EQ(popped.depth, 3);
    EXPECT_EQ(popped.parentId, "parent-1");
}
