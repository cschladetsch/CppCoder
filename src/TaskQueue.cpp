#include "cppcoder/TaskQueue.h"

namespace cppcoder {

bool TaskQueue::Push(const Task& task) {
    if (task.repeatable) {
        // Repeatable tasks own a set of sub-areas; only dedupe on the
        // umbrella target area so we don't lose the repeat targets.
        if (visited_.count(task.targetArea) || queuedAreas_.count(task.targetArea)) {
            return false;
        }
        queuedAreas_.insert(task.targetArea);
        queue_.push_back(task);
        return true;
    }

    if (visited_.count(task.targetArea) || queuedAreas_.count(task.targetArea)) {
        return false;
    }
    queuedAreas_.insert(task.targetArea);
    queue_.push_back(task);
    return true;
}

Task TaskQueue::Pop() {
    Task t = queue_.front();
    queue_.pop_front();
    queuedAreas_.erase(t.targetArea);
    return t;
}

void TaskQueue::MarkVisited(const std::string& area) { visited_.insert(area); }

bool TaskQueue::Visited(const std::string& area) const { return visited_.count(area) > 0; }

}  // namespace cppcoder
