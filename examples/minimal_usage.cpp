// examples/minimal_usage.cpp
//
// Shows the library pieces that work without a running Ollama instance:
// keyword fallback extraction, codebase scanning, and the task queue's
// dedup/visited bookkeeping. Useful as a "getting started" reference
// and as a smoke test that the library links correctly on its own.
//
// Usage: minimal_usage [path-to-scan]   (defaults to the current directory)

#include "cppcoder/CodebaseScanner.h"
#include "cppcoder/ResearchEngine.h"
#include "cppcoder/TaskQueue.h"
#include "cppcoder/Types.h"

#include <iostream>

int main(int argc, char** argv) {
    std::string root = argc > 1 ? argv[1] : ".";

    std::cout << "== FallbackKeywords ==\n";
    std::string question = "How does the ResearchEngine decide when to stop investigating?";
    auto keywords = cppcoder::FallbackKeywords(question, 5);
    std::cout << "question: " << question << "\nkeywords:";
    for (const auto& k : keywords) std::cout << " " << k;
    std::cout << "\n\n";

    std::cout << "== CodebaseScanner ==\n";
    cppcoder::CodebaseScanner scanner(root);
    auto matches = scanner.FindFilesMatchingKeyword("ResearchEngine", 10);
    std::cout << "files under '" << root << "' mentioning 'ResearchEngine':\n";
    for (const auto& m : matches) std::cout << "  " << m << "\n";
    if (matches.empty()) std::cout << "  (none -- try running from the repo root)\n";
    std::cout << "\n";

    std::cout << "== TaskQueue ==\n";
    cppcoder::TaskQueue queue;
    cppcoder::Task t1{"t1", "src/foo.cpp", "find foo", "foo is documented"};
    cppcoder::Task t2{"t2", "src/bar.cpp", "find bar", "bar is documented"};
    cppcoder::Task dup{"t3", "src/foo.cpp", "find foo again", "n/a"};

    std::cout << "push t1 (src/foo.cpp): " << (queue.Push(t1) ? "accepted" : "rejected") << "\n";
    std::cout << "push t2 (src/bar.cpp): " << (queue.Push(t2) ? "accepted" : "rejected") << "\n";
    std::cout << "push dup (src/foo.cpp again): " << (queue.Push(dup) ? "accepted" : "rejected")
               << "  (should be rejected -- area already queued)\n";
    std::cout << "queue size: " << queue.Size() << "\n";

    auto popped = queue.Pop();
    queue.MarkVisited(popped.targetArea);
    std::cout << "popped: " << popped.id << " (" << popped.targetArea << "), marked visited\n";

    cppcoder::Task revisit{"t4", popped.targetArea, "try again", "n/a"};
    std::cout << "push t4 (" << popped.targetArea
               << ", now visited): " << (queue.Push(revisit) ? "accepted" : "rejected")
               << "  (should be rejected)\n";

    return 0;
}
