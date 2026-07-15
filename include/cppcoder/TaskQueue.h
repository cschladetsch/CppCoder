#pragma once

#include "cppcoder/Types.h"

#include <deque>
#include <unordered_set>

namespace cppcoder {

// FIFO queue of pending tasks with area-visited tracking, so the engine
// never investigates the same area twice and simply drops tasks aimed
// at areas already covered.
class TaskQueue {
public:
    // Returns false (and does not enqueue) if the task's target area has
    // already been visited or is already queued.
    bool Push(const Task& task);

    bool Empty() const { return queue_.empty(); }
    std::size_t Size() const { return queue_.size(); }

    Task Pop();

    void MarkVisited(const std::string& area);
    bool Visited(const std::string& area) const;

private:
    std::deque<Task> queue_;
    std::unordered_set<std::string> visited_;
    std::unordered_set<std::string> queuedAreas_;
};

}  // namespace cppcoder
