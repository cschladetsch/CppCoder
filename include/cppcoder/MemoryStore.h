#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace cppcoder {

// Small persisted list of facts about the user (name, age, preferences,
// ...), auto-detected from chat messages (see FactExtractor.h) and
// injected back into every conversation's system prompt by ChatServer,
// so the assistant "remembers" them across sessions and model switches.
//
// Stored as JSON at, in priority order:
//   1. $CPPCODER_MEMORY_FILE, if set
//   2. $DEEPSEEK_MODEL_HOME/memory.json, if that's set (matches the
//      shared-model-store convention used elsewhere in this project)
//   3. ~/.models/memory.json otherwise
//
// Thread-safe: ChatServer's httplib::Server handles requests from a
// thread pool, and every public method here takes the same mutex.
class MemoryStore {
public:
    // Uses ResolveDefaultPath() if `path` is empty.
    explicit MemoryStore(std::string path = {});

    // Path actually in use (resolved at construction time).
    const std::string& path() const { return path_; }

    // All stored facts, oldest first.
    std::vector<std::string> AllFacts() const;

    // Appends `fact` (trimmed) if it isn't already present (case-
    // insensitive comparison) and persists to disk immediately. Returns
    // true if it was newly added, false if it was empty or a duplicate.
    bool AddFact(const std::string& fact);

    // Removes the first fact matching `fact` case-insensitively.
    // Persists to disk immediately. Returns true if something was
    // removed.
    bool RemoveFact(const std::string& fact);

    static std::string ResolveDefaultPath();

private:
    void Load();
    void SaveLocked() const;  // caller must hold mutex_

    std::string path_;
    mutable std::mutex mutex_;
    std::vector<std::string> facts_;
};

}  // namespace cppcoder
