#pragma once

#include "cppcoder/MemoryStore.h"

#include <string>

namespace cppcoder {

struct ChatServerConfig {
    // Address/port the chat web UI + API are served on.
    std::string bindHost = "127.0.0.1";
    int bindPort = 8765;

    // Ollama connection used to service /api/models and /api/chat.
    std::string ollamaHost = "localhost";
    int ollamaPort = 11434;
    std::string defaultModel = "qwen2.5-coder:7b";

    // Directory containing chat.html (and friends) to serve as static
    // files at "/". Resolved by main.cpp before construction.
    std::string webRoot;

    // Path to the persisted facts file (see MemoryStore). Empty means
    // "use MemoryStore::ResolveDefaultPath()".
    std::string memoryFilePath;
};

// Minimal local web server backing the "Claude for Desktop"-style chat
// UI (web/chat.html): serves the static frontend and proxies chat turns
// straight through to Ollama's /api/chat (no research engine involved --
// this is plain conversational chat with swappable models).
//
// Also owns a MemoryStore: every user message is scanned for durable
// facts (name, age, ...), which get persisted and re-injected as a
// system message on every subsequent turn, so the assistant "remembers"
// them across conversations and model switches.
class ChatServer {
public:
    explicit ChatServer(ChatServerConfig config);

    // Blocks the calling thread serving requests until the process is
    // interrupted (Ctrl+C). Returns a process exit code.
    int Run();

private:
    ChatServerConfig config_;
    MemoryStore memory_;
};

}  // namespace cppcoder
