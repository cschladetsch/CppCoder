#include "ModelStore.hpp"

#include <cstdlib>
#include <filesystem>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

TEST(ModelStoreTests, ResolveAndCreate) {
  const char* kEnv = "DEEPSEEK_MODEL_HOME";
  const std::string base = "/tmp/deepseek_models_test";
  ::setenv(kEnv, base.c_str(), 1);

  const std::string model = "deepseek-r1";
  const std::string expected = base + "/" + model;
  EXPECT_EQ(deepseek::ModelStore::ResolveModelPath(model), expected);

  std::string error;
  auto created = deepseek::ModelStore::EnsureModelDir(model, &error);
  ASSERT_TRUE(created.has_value()) << error;
  EXPECT_TRUE(deepseek::ModelStore::ModelExists(model));

  std::error_code ec;
  fs::remove_all(base, ec);
  EXPECT_FALSE(ec);

  ::unsetenv(kEnv);
}

TEST(ModelStoreTests, SanitizesColonsInModelNameForWindowsCompatibility) {
  // Registry-style model tags (e.g. Ollama's "llama3:8b") contain ':',
  // which NTFS treats as alternate-data-stream syntax rather than a
  // normal path character. ResolveModelPath must sanitize it so the
  // resulting path is a real, creatable directory on every platform.
  const char* kEnv = "DEEPSEEK_MODEL_HOME";
  const std::string base = "/tmp/deepseek_models_test_colon";
  ::setenv(kEnv, base.c_str(), 1);

  const std::string model = "qwen2.5-coder:7b";
  const std::string expected = base + "/qwen2.5-coder_7b";
  EXPECT_EQ(deepseek::ModelStore::ResolveModelPath(model), expected);

  std::string error;
  auto created = deepseek::ModelStore::EnsureModelDir(model, &error);
  ASSERT_TRUE(created.has_value()) << error;
  EXPECT_TRUE(deepseek::ModelStore::ModelExists(model));

  std::error_code ec;
  fs::remove_all(base, ec);
  EXPECT_FALSE(ec);

  ::unsetenv(kEnv);
}
