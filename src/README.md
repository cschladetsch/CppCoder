# src/

Implementation of every header in `include/cppcoder/`, plus `main.cpp`
(the single CLI entry point for both research mode and chat mode).
Builds into one static library, `cppcoder_core`, linked by the
`cppcoder` executable, `tests/cppcoder_tests`, and `examples/minimal_usage`.

## Files

| File | Implements | Notes |
|---|---|---|
| `JsonUtil.cpp` | `ExtractJsonObject`/`ExtractJsonArray` | Brace/bracket-depth scanning, no JSON parsing involved (the input isn't guaranteed to be valid JSON yet). |
| `Logging.cpp` | `InitLogging` | Maps a level name string to spdlog's enum; sets up console + optional file sink. |
| `CodebaseScanner.cpp` | `CodebaseScanner` | Recursive directory walk, extension filtering, `.git`/`build` exclusion, token-budget-aware truncation. |
| `OllamaClient.cpp` | `OllamaClient` | Synchronous `httplib::Client` calls to `/api/generate` and `/api/tags`. One client per call site; cheap to construct. |
| `Worker.cpp` | `Worker` | Builds the worker prompt, calls `OllamaClient::Generate`, parses the JSON response (`ParseWorkerResponse`) into a `Finding`. |
| `Judge.cpp` | `Judge` | Builds the judge prompt, calls the model, applies the response (`ApplyJudgeResponse`) to prune the `Finding`. |
| `TaskQueue.cpp` | `TaskQueue` | `std::deque` + two `unordered_set`s (queued areas, visited areas) for O(1) dedup checks. |
| `ResearchEngine.cpp` | `ResearchEngine` | The orchestration loop described in the root README; also `FallbackKeywords`. |
| `ChatServer.cpp` | `ChatServer` | httplib **server**: static file serving, `/api/models`, `/api/memory` (GET/POST/DELETE), `/api/chat` (streaming proxy to Ollama). |
| `MemoryStore.cpp` | `MemoryStore` | JSON read/write of the facts file, mutex-guarded, case-insensitive dedup. |
| `FactExtractor.cpp` | `ExtractFacts` | The regex pattern table -- see comments there before adding a new phrasing. |
| `main.cpp` | CLI entry point | Argument parsing, then dispatches to either the research loop or `ChatServer::Run()`. |

## Build targets

```mermaid
flowchart TD
    subgraph lib["cppcoder_core (STATIC)"]
        JsonUtil.cpp
        Logging.cpp
        OllamaClient.cpp
        CodebaseScanner.cpp
        Worker.cpp
        Judge.cpp
        TaskQueue.cpp
        ResearchEngine.cpp
        ChatServer.cpp
        MemoryStore.cpp
        FactExtractor.cpp
    end

    lib -->|PUBLIC| NJ[("nlohmann_json")]
    lib -->|PUBLIC| ModelStore[("ModelStore::ModelStore")]
    lib -->|PUBLIC| Spdlog[("spdlog::spdlog")]
    lib -->|PRIVATE| Httplib[("httplib::httplib")]
    lib -->|PRIVATE| Threads[("Threads::Threads")]

    main.cpp --> lib
    CLI["cppcoder (executable)"] --> main.cpp
```

`nlohmann_json`/`ModelStore`/`spdlog` are linked `PUBLIC` because
`cppcoder_core` is a **static** library: PRIVATE dependencies of a
static lib don't automatically propagate to whatever finally links it
(a real CMake gotcha), so anything a consumer's final link step needs
has to be PUBLIC here. `httplib` and `Threads` stay `PRIVATE` since
nothing outside `cppcoder_core`'s own `.cpp` files touches them
directly.

## Two entry points, one binary

`main.cpp` parses `argv` once and then takes one of two paths:

- No `--serve`: requires `--question`/`--codebase`, runs
  `ResearchEngine::Research()`, prints the answer, exits.
- `--serve`: builds a `ChatServerConfig` (resolving `--web-root` via
  `ResolveDefaultWebRoot()` if not given explicitly) and calls
  `ChatServer::Run()`, which blocks serving HTTP until Ctrl+C.

There's no shared state between the two paths beyond `OllamaConfig`
(host/port/model) -- see `include/cppcoder/README.md` for the header-level
picture, and the root README's [Chat mode](../README.md#chat-mode)
section for the request-level sequence diagram.
