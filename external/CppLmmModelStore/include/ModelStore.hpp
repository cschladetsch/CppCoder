#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace deepseek {

class ModelStore {
 public:
  static std::string ResolveModelHome();
  static std::string ResolveModelPath(std::string_view model_name);

  // Ensures the model directory exists and returns its path.
  // If creation fails, returns std::nullopt and optionally sets error_out.
  static std::optional<std::string> EnsureModelDir(std::string_view model_name,
                                                   std::string* error_out = nullptr);

  // Returns true if the model directory exists.
  static bool ModelExists(std::string_view model_name);
};

}  // namespace deepseek
