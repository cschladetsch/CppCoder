# include/cppcoder/

Public headers for the `cppcoder_core` library. Everything here is
namespaced under `cppcoder` (except `deepseek::ModelStore`, which lives
in the `external/CppLmmModelStore` submodule and is included directly as
`<ModelStore.hpp>`).

Two roughly independent groups of headers live here: the **research
engine** (question in, answer out, no HTTP server involved) and the
**chat server** (a small local web app on top of the same `OllamaClient`).
`main.cpp` is the only thing that wires both to a CLI flag (`--question`
vs `--serve`).

## Research engine headers

| Header | Purpose |
|---|---|
| `Types.h` | `Task`, `Finding`, `WorkerOutcome`, `EstimateTokens()` -- the shared vocabulary every other research-engine header builds on. |
| `TaskQueue.h` | FIFO queue with area-dedup: rejects a `Task` if its `targetArea` is already queued or already visited. |
| `CodebaseScanner.h` | Reads source files under a root/target area within a token budget; also does a cheap keyword grep used to seed initial tasks. |
| `OllamaClient.h` | Thin synchronous wrapper around Ollama's `/api/generate` (used by `Worker`/`Judge`) and `/api/tags` (`IsModelAvailable`, `ListModels`). |
| `Worker.h` | Executes one `Task`: gathers context via `CodebaseScanner`, calls the model, parses the response into a `Finding`. `ParseWorkerResponse` is pure/static and unit-testable without Ollama running. |
| `Judge.h` | Reviews a `Worker`'s `Finding` against the original question and prunes off-topic content/directions. `ApplyJudgeResponse` is pure/static, same testability story as `Worker`. |
| `ResearchEngine.h` | Orchestrates the whole loop: keyword extraction → seed task → worker/judge loop → answer synthesis. Owns the `EventSink` used to emit JSON-Lines events for `web/index.html`. |
| `JsonUtil.h` | `ExtractJsonObject`/`ExtractJsonArray` -- pull the outermost `{...}`/`[...]` span out of model output that's wrapped in prose or markdown fences. |
| `Logging.h` | `InitLogging()` -- configures the default spdlog logger (console + optional file sink) used everywhere in this library. |

## Chat server headers

| Header | Purpose |
|---|---|
| `ChatServer.h` | Local HTTP server (cpp-httplib) serving `web/chat.html` and proxying `/api/chat` straight through to Ollama, streaming NDJSON back. Owns a `MemoryStore`. |
| `MemoryStore.h` | Thread-safe persisted fact list (`~/.models/memory.json` by default) with case-insensitive dedup on add. |
| `FactExtractor.h` | `ExtractFacts(message)` -- regex heuristics that pull durable facts ("my name is...", "I am NN yo") out of a single chat message. |

## Class diagram

```mermaid
classDiagram
    class Task {
        +string id
        +string targetArea
        +string researchGoal
        +string successCriteria
        +int depth
        +bool repeatable
        +string[] repeatTargets
        +string parentId
    }
    class Finding {
        +WorkerOutcome outcome
        +string areaInvestigated
        +string summary
        +Task[] suggestedDirections
        +milliseconds duration
        +size_t promptTokensApprox
    }
    class WorkerOutcome {
        <<enumeration>>
        Success
        NoInformation
        PartialWithDirections
    }
    Finding --> WorkerOutcome
    Finding --> "0..*" Task : suggestedDirections

    class TaskQueue {
        +bool Push(Task)
        +Task Pop()
        +bool Empty()
        +size_t Size()
        +void MarkVisited(string)
        +bool Visited(string)
    }
    TaskQueue --> Task

    class CodebaseScanner {
        +ScanResult Scan(targetArea, tokenBudget)
        +string[] FindFilesMatchingKeyword(keyword, maxResults)
    }

    class OllamaClient {
        +optional~string~ Generate(prompt, systemPrompt)
        +bool IsModelAvailable()
        +string[] ListModels()
    }

    class Worker {
        +Finding Execute(Task)
        +Finding ParseWorkerResponse(raw, area)$
    }
    Worker --> OllamaClient
    Worker --> CodebaseScanner
    Worker --> Finding

    class Judge {
        +Finding Review(topic, Finding)
        +Finding ApplyJudgeResponse(raw, Finding)$
    }
    Judge --> OllamaClient

    class ResearchEngine {
        +ResearchResult Research(question)
        +Task[] SeedInitialTasks(question, keywords)
        +void SetEventSink(EventSink)
    }
    ResearchEngine --> Worker
    ResearchEngine --> Judge
    ResearchEngine --> TaskQueue
    ResearchEngine --> CodebaseScanner

    class ChatServerConfig {
        +string bindHost
        +int bindPort
        +string ollamaHost
        +int ollamaPort
        +string defaultModel
        +string webRoot
        +string memoryFilePath
    }
    class ChatServer {
        +int Run()
    }
    class MemoryStore {
        +string[] AllFacts()
        +bool AddFact(string)
        +bool RemoveFact(string)
        +string ResolveDefaultPath()$
    }
    ChatServer --> ChatServerConfig
    ChatServer --> MemoryStore
    ChatServer ..> FactExtractor : ExtractFacts()
    ChatServer --> OllamaClient : proxies to
```

## Conventions worth knowing

- **Pure-function testability.** `Worker::ParseWorkerResponse`,
  `Judge::ApplyJudgeResponse`, `ResearchEngine::FallbackKeywords`/`SeedInitialTasks`
  are all `static` or free functions with no I/O, specifically so
  `tests/` can exercise the model-response-parsing logic without a
  running Ollama instance. Keep new parsing logic in this shape if you
  add to it.
- **`EventSink`** (`ResearchEngine.h`) is a `std::function<void(const
  std::string&)>` called once per already-serialized JSON event line.
  `main.cpp` wires it to a file when `--events-file` is given; that file
  is what `web/index.html` and `examples/replay_demo` consume.
- **Header include weight.** `ResearchEngine.h` pulls in every other
  research-engine header; `ChatServer.h` only needs `MemoryStore.h`.
  There's no header that depends on both groups except `main.cpp` itself.
