#include "ModelStore.hpp"

#include <cstdlib>
#include <filesystem>

namespace deepseek {
namespace {

std::string ResolveModelHomeInternal() {
  if (const char* override_dir = std::getenv("DEEPSEEK_MODEL_HOME"); override_dir &&
      std::string(override_dir).size() > 0) {
    return override_dir;
  }
  const char* xdg_data_home = std::getenv("XDG_DATA_HOME");
  const char* home = std::getenv("HOME");
  std::string base = (xdg_data_home && std::string(xdg_data_home).size() > 0)
                         ? xdg_data_home
                         : (home ? std::string(home) + "/.local/share" : ".");
  return base + "/deepseek/models";
}

// Windows forbids '<', '>', ':', '"', '|', '?', '*', and '\' inside a path
// component -- ':' is the one that actually bites in practice, since NTFS
// reserves it for alternate-data-stream syntax ("name:stream"). Model
// identifiers from registries such as Ollama routinely look like
// "llama3:8b", which POSIX filesystems accept verbatim but Windows does
// not: ModelExists()/EnsureModelDir() would silently address an ADS (or
// fail outright) instead of the intended directory. Sanitizing here means
// every caller of ResolveModelPath (and therefore ModelExists and
// EnsureModelDir, which both go through it) gets a path that behaves the
// same on every platform.
std::string SanitizePathComponent(std::string_view component) {
  std::string sanitized;
  sanitized.reserve(component.size());
  for (char c : component) {
    switch (c) {
      case ':':
      case '*':
      case '?':
      case '"':
      case '<':
      case '>':
      case '|':
      case '\\':
        sanitized.push_back('_');
        break;
      default:
        sanitized.push_back(c);
    }
  }
  return sanitized;
}

}  // namespace

std::string ModelStore::ResolveModelHome() { return ResolveModelHomeInternal(); }

std::string ModelStore::ResolveModelPath(std::string_view model_name) {
  std::string home = ResolveModelHomeInternal();
  if (!home.empty() && home.back() != '/') {
    home.push_back('/');
  }
  return home + SanitizePathComponent(model_name);
}

std::optional<std::string> ModelStore::EnsureModelDir(std::string_view model_name,
                                                      std::string* error_out) {
  std::string path = ResolveModelPath(model_name);
  std::error_code ec;
  std::filesystem::create_directories(path, ec);
  if (ec) {
    if (error_out) {
      *error_out = "Failed to create model dir: " + path + " (" + ec.message() + ")";
    }
    return std::nullopt;
  }
  return path;
}

bool ModelStore::ModelExists(std::string_view model_name) {
  std::string path = ResolveModelPath(model_name);
  std::error_code ec;
  return std::filesystem::exists(path, ec);
}

}  // namespace deepseek
