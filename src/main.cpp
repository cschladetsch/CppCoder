#include "cppcoder/CodebaseScanner.h"
#include "cppcoder/Logging.h"
#include "cppcoder/OllamaClient.h"
#include "cppcoder/ResearchEngine.h"

#include <ModelStore.hpp>
#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void PrintUsage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " --question \"...\" --codebase <path> [options]\n"
        << "\n"
        << "Options:\n"
        << "  --question <text>        Question to research (required)\n"
        << "  --codebase <path>        Root of the codebase to investigate (required)\n"
        << "  --model <name>           Ollama model tag (default: qwen2.5-coder:7b)\n"
        << "  --host <host>            Ollama host (default: localhost)\n"
        << "  --port <port>            Ollama port (default: 11434)\n"
        << "  --max-minutes <n>        Wall clock budget in minutes (default: 90)\n"
        << "  --max-iterations <n>     Max task loop iterations (default: 200)\n"
        << "  --token-budget <n>       Approx tokens per task (default: 120000)\n"
        << "  --events-file <path>     Write JSON-lines engine events for the web UI\n"
        << "  --log-level <level>      trace|debug|info|warn|err|critical|off (default: info)\n"
        << "  --log-file <path>        Also write logs to this file\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string question;
    std::string codebasePath;
    std::string eventsFilePath;
    std::string logLevel = "info";
    std::string logFilePath;
    cppcoder::OllamaConfig ollamaConfig;
    cppcoder::EngineConfig engineConfig;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << "\n";
                std::exit(1);
            }
            return argv[++i];
        };

        if (arg == "--question") {
            question = next("--question");
        } else if (arg == "--codebase") {
            codebasePath = next("--codebase");
        } else if (arg == "--model") {
            ollamaConfig.model = next("--model");
        } else if (arg == "--host") {
            ollamaConfig.host = next("--host");
        } else if (arg == "--port") {
            ollamaConfig.port = std::stoi(next("--port"));
        } else if (arg == "--max-minutes") {
            engineConfig.maxWallClock = std::chrono::minutes(std::stoi(next("--max-minutes")));
        } else if (arg == "--max-iterations") {
            engineConfig.maxIterations = std::stoi(next("--max-iterations"));
        } else if (arg == "--token-budget") {
            engineConfig.tokenBudgetPerTask = std::stoull(next("--token-budget"));
        } else if (arg == "--events-file") {
            eventsFilePath = next("--events-file");
        } else if (arg == "--log-level") {
            logLevel = next("--log-level");
        } else if (arg == "--log-file") {
            logFilePath = next("--log-file");
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    cppcoder::InitLogging(logLevel, logFilePath);

    if (question.empty() || codebasePath.empty()) {
        PrintUsage(argv[0]);
        return 1;
    }

    // Zero-model-duplication convention: verify the requested model
    // resolves in the shared model store before spending any research
    // time on it. Ollama manages its own runtime, so this is advisory
    // only -- it does not block the run.
    if (!deepseek::ModelStore::ModelExists(ollamaConfig.model)) {
        spdlog::info(
            "'{}' not found under shared model store ({}). Continuing -- Ollama manages its "
            "own model storage separately.",
            ollamaConfig.model, deepseek::ModelStore::ResolveModelPath(ollamaConfig.model));
    }

    cppcoder::OllamaClient client(ollamaConfig);
    if (!client.IsModelAvailable()) {
        spdlog::warn(
            "Ollama at {}:{} does not report model '{}' as available. Run `ollama pull {}` "
            "first. Continuing anyway.",
            ollamaConfig.host, ollamaConfig.port, ollamaConfig.model, ollamaConfig.model);
    }

    cppcoder::CodebaseScanner scanner(codebasePath);
    cppcoder::ResearchEngine engine(std::move(client), std::move(scanner), engineConfig);

    std::ofstream eventsFile;
    if (!eventsFilePath.empty()) {
        eventsFile.open(eventsFilePath, std::ios::out | std::ios::trunc);
        if (!eventsFile) {
            spdlog::warn("Could not open events file '{}' for writing; continuing without "
                         "event output.",
                         eventsFilePath);
        } else {
            engine.SetEventSink([&eventsFile](const std::string& line) {
                eventsFile << line << "\n";
                eventsFile.flush();
            });
        }
    }

    spdlog::info("Researching: {}", question);
    cppcoder::ResearchResult result = engine.Research(question);

    std::cout << "\n=== Research complete ===\n"
              << "Answered: " << (result.answered ? "yes" : "no") << "\n"
              << "Termination: " << result.terminationReason << "\n"
              << "Iterations: " << result.iterationsRun << "\n"
              << "Wall clock: " << result.wallClock.count() << " ms\n"
              << "Successful findings: " << result.successfulFindings.size() << "\n\n";

    if (result.answered) {
        std::cout << "Answer:\n" << result.answer << "\n";
    } else {
        std::cout << "No answer produced. Findings gathered along the way:\n";
        for (const auto& f : result.successfulFindings) {
            std::cout << "- [" << f.areaInvestigated << "] " << f.summary << "\n";
        }
    }

    return result.answered ? 0 : 2;
}
