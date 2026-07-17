#include "cppcoder/MemoryStore.h"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace cppcoder {

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace {

std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}  // namespace

std::string MemoryStore::ResolveDefaultPath() {
    if (const char* override_path = std::getenv("CPPCODER_MEMORY_FILE");
        override_path && std::string(override_path).size() > 0) {
        return override_path;
    }
    if (const char* model_home = std::getenv("DEEPSEEK_MODEL_HOME");
        model_home && std::string(model_home).size() > 0) {
        return (fs::path(model_home) / "memory.json").string();
    }
#if defined(_WIN32)
    const char* home = std::getenv("USERPROFILE");
#else
    const char* home = std::getenv("HOME");
#endif
    std::string base = (home && std::string(home).size() > 0) ? home : ".";
    return (fs::path(base) / ".models" / "memory.json").string();
}

MemoryStore::MemoryStore(std::string path)
    : path_(path.empty() ? ResolveDefaultPath() : std::move(path)) {
    Load();
}

void MemoryStore::Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    facts_.clear();

    std::ifstream in(path_);
    if (!in) {
        spdlog::debug("MemoryStore: no existing memory file at {}, starting empty", path_);
        return;
    }
    try {
        json parsed;
        in >> parsed;
        for (const auto& f : parsed.value("facts", json::array())) {
            if (f.is_string()) {
                facts_.push_back(f.get<std::string>());
            } else if (f.is_object()) {
                facts_.push_back(f.value("text", std::string{}));
            }
        }
        spdlog::debug("MemoryStore: loaded {} fact(s) from {}", facts_.size(), path_);
    } catch (const json::exception& e) {
        spdlog::warn("MemoryStore: failed to parse {}: {} -- starting empty", path_, e.what());
        facts_.clear();
    }
}

void MemoryStore::SaveLocked() const {
    std::error_code ec;
    fs::path parent = fs::path(path_).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            spdlog::error("MemoryStore: could not create directory for {}: {}", path_,
                          ec.message());
            return;
        }
    }

    json out;
    out["facts"] = json::array();
    for (const auto& f : facts_) {
        out["facts"].push_back(f);
    }

    std::ofstream file(path_, std::ios::out | std::ios::trunc);
    if (!file) {
        spdlog::error("MemoryStore: could not open {} for writing", path_);
        return;
    }
    file << out.dump(2);
    spdlog::debug("MemoryStore: saved {} fact(s) to {}", facts_.size(), path_);
}

std::vector<std::string> MemoryStore::AllFacts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return facts_;
}

bool MemoryStore::AddFact(const std::string& fact) {
    std::string trimmed = Trim(fact);
    if (trimmed.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    std::string lowered = ToLower(trimmed);
    for (const auto& existing : facts_) {
        if (ToLower(existing) == lowered) {
            return false;  // already known
        }
    }
    facts_.push_back(trimmed);
    SaveLocked();
    spdlog::info("MemoryStore: remembered new fact: {}", trimmed);
    return true;
}

bool MemoryStore::RemoveFact(const std::string& fact) {
    std::string lowered = ToLower(Trim(fact));
    if (lowered.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(facts_.begin(), facts_.end(), [&](const std::string& existing) {
        return ToLower(existing) == lowered;
    });
    if (it == facts_.end()) {
        return false;
    }
    facts_.erase(it);
    SaveLocked();
    spdlog::info("MemoryStore: forgot fact: {}", fact);
    return true;
}

}  // namespace cppcoder
