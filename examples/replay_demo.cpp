// examples/replay_demo.cpp
//
// Replays a JSON-Lines event log produced by `cppcoder --events-file`
// (or the bundled examples/demo_events.jsonl) directly in the terminal.
// This is the CLI counterpart to web/index.html: same event schema,
// so anything recorded from a real run works in both.
//
// Usage:
//   replay_demo --events examples/demo_events.jsonl --step
//   replay_demo --events examples/demo_events.jsonl --speed 4
//   replay_demo --events /tmp/run.jsonl --speed 0.5   (half speed)

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace {

constexpr const char* kReset = "\033[0m";
constexpr const char* kBold = "\033[1m";
constexpr const char* kDim = "\033[2m";
constexpr const char* kBlue = "\033[34m";
constexpr const char* kGreen = "\033[32m";
constexpr const char* kRed = "\033[31m";
constexpr const char* kYellow = "\033[33m";
constexpr const char* kCyan = "\033[36m";

bool g_color = true;

std::string C(const char* code) { return g_color ? code : ""; }

void PrintUsage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " --events <path.jsonl> [options]\n\n"
        << "Options:\n"
        << "  --events <path>   JSON-Lines event log to replay (required)\n"
        << "  --step             Wait for Enter between events\n"
        << "  --speed <n>        Playback speed multiplier (default 1.0; 2.0 = twice as\n"
        << "                     fast, 0.5 = half speed). Ignored with --step.\n"
        << "  --no-color         Disable ANSI colour output\n";
}

std::vector<json> LoadEvents(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Could not open events file: " << path << "\n";
        std::exit(1);
    }
    std::vector<json> events;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (line.find_first_not_of(" \t\r\n") == std::string::npos) continue;
        try {
            events.push_back(json::parse(line));
        } catch (const json::exception& e) {
            std::cerr << "Skipping malformed line " << lineNo << ": " << e.what() << "\n";
        }
    }
    return events;
}

std::string ShortLabel(const std::string& area) {
    auto pos = area.find_last_of('/');
    return pos == std::string::npos ? area : area.substr(pos + 1);
}

void WaitForStep() {
    std::cout << C(kDim) << "  -- press Enter to continue --" << C(kReset);
    std::cout.flush();
    std::cin.get();
}

void SleepScaled(int baseMs, double speed) {
    if (speed <= 0) speed = 1.0;
    auto ms = static_cast<int>(baseMs / speed);
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Renders one event as a terminal line/block. Returns a suggested base
// delay in ms to pace automatic playback (ignored in --step mode).
int RenderEvent(const json& evt) {
    std::string type = evt.value("event", "");

    if (type == "question") {
        std::cout << "\n" << C(kBold) << C(kCyan) << "QUESTION  " << C(kReset)
                   << evt.value("question", "") << "\n";
        return 400;
    }
    if (type == "keywords_extracted") {
        std::cout << C(kDim) << "  keywords: " << C(kReset);
        bool first = true;
        for (const auto& k : evt.value("keywords", json::array())) {
            if (!first) std::cout << ", ";
            std::cout << C(kBlue) << k.get<std::string>() << C(kReset);
            first = false;
        }
        std::cout << "\n";
        return 400;
    }
    if (type == "task_queued") {
        std::cout << C(kDim) << "  + queued  " << C(kReset) << evt.value("id", "") << "  "
                   << C(kDim) << ShortLabel(evt.value("target_area", "")) << C(kReset) << "\n";
        return 150;
    }
    if (type == "task_started") {
        std::cout << C(kBlue) << ">> running " << C(kReset) << C(kBold) << evt.value("id", "")
                   << C(kReset) << "  " << ShortLabel(evt.value("target_area", "")) << " ...\n";
        return 500;
    }
    if (type == "worker_result") {
        std::string outcome = evt.value("outcome", "");
        const char* color = outcome == "success" ? kGreen : (outcome == "no_information" ? kDim : kYellow);
        std::cout << "   worker:  " << C(color) << outcome << C(kReset) << C(kDim) << "  ("
                   << evt.value("duration_ms", 0) << "ms, "
                   << evt.value("directions", json::array()).size() << " direction(s) proposed)"
                   << C(kReset) << "\n";
        return 300;
    }
    if (type == "judge_result") {
        auto kept = evt.value("kept_directions", json::array());
        int rejected = evt.value("rejected_count", 0);
        std::cout << "   judge:   kept=" << C(kGreen) << kept.size() << C(kReset)
                   << "  rejected=" << (rejected > 0 ? C(kRed) : C(kDim)) << rejected << C(kReset)
                   << "\n";
        for (const auto& d : kept) {
            std::cout << C(kDim) << "              -> " << C(kReset)
                       << ShortLabel(d.value("target_area", "")) << "\n";
        }
        return 300;
    }
    if (type == "answer_progress") {
        std::cout << C(kGreen) << "   [draft answer] " << C(kReset)
                   << evt.value("summary", "") << "\n";
        return 400;
    }
    if (type == "answer_found") {
        std::string answer = evt.value("answer", "");
        std::cout << "\n" << C(kBold) << C(kGreen) << "ANSWER FOUND" << C(kReset) << "\n  "
                   << answer << "\n";
        return 700;
    }
    if (type == "complete") {
        bool answered = evt.value("answered", false);
        std::cout << "\n"
                   << C(kBold) << (answered ? C(kGreen) : C(kRed)) << "DONE" << C(kReset) << "  "
                   << (answered ? "answered" : "no answer") << C(kDim) << "  ("
                   << evt.value("iterations", 0) << " iteration(s), "
                   << (evt.value("wall_clock_ms", 0LL) / 1000.0) << "s)  reason: "
                   << evt.value("reason", "") << C(kReset) << "\n\n";
        return 0;
    }

    std::cout << C(kDim) << "  (unrecognised event: " << type << ")" << C(kReset) << "\n";
    return 100;
}

}  // namespace

int main(int argc, char** argv) {
    std::string eventsPath;
    bool step = false;
    double speed = 1.0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << "\n";
                std::exit(1);
            }
            return argv[++i];
        };

        if (arg == "--events") {
            eventsPath = next("--events");
        } else if (arg == "--step") {
            step = true;
        } else if (arg == "--speed") {
            speed = std::stod(next("--speed"));
        } else if (arg == "--no-color") {
            g_color = false;
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (eventsPath.empty()) {
        PrintUsage(argv[0]);
        return 1;
    }

    auto events = LoadEvents(eventsPath);
    if (events.empty()) {
        std::cerr << "No valid events found in " << eventsPath << "\n";
        return 1;
    }

    std::cout << C(kDim) << "Replaying " << events.size() << " event(s) from " << eventsPath
               << (step ? "  [step mode]" : "") << C(kReset) << "\n";

    for (std::size_t i = 0; i < events.size(); ++i) {
        int baseDelay = RenderEvent(events[i]);
        bool last = (i + 1 == events.size());
        if (last) break;
        if (step) {
            WaitForStep();
        } else if (baseDelay > 0) {
            SleepScaled(baseDelay, speed);
        }
    }

    return 0;
}
