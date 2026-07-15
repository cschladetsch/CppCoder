#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace deepseek {

class DeepSeekStreamParser {
 public:
  using DeltaCallback =
      std::function<void(std::string_view reasoning_delta, std::string_view content_delta)>;

  explicit DeepSeekStreamParser(DeltaCallback on_delta);

  // Feeds a raw stream chunk. Returns false on parse error.
  bool Feed(std::string_view chunk, std::string* error_out = nullptr);

 private:
  DeltaCallback on_delta_;
  std::string buffer_;
};

}  // namespace deepseek
