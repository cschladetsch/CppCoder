#include "ModelStore.hpp"

#include <cstdlib>
#include <filesystem>

#include <gtest/gtest.h>

namespace fs = std::filesystem;

namespace {

// setenv/unsetenv are POSIX-only; MSVC's CRT (including under clang-cl)
// doesn't provide them at all. _putenv_s is the portable-on-Windows
// equivalent, and passing "" as the value removes the variable, matching
// unsetenv's effect -- ResolveModelHomeInternal() already treats an empty
// override as "not set" too, so behavior stays identical either way.
void SetEnvVar(const char* name, const std::string& value) {
#if defined(_WIN32)
  _putenv_s(name, value.c_str());
#else
  ::setenv(name, value.c_str(), 1);
#endif
}

void UnsetEnvVar(const char* name) {
#if defined(_WIN32)
  _putenv_s(name, "");
#else
  ::unsetenv(name);
#endif
}

}  // namespace

TEST(ModelStoreTests, ResolveAndCreate) {
  const char* kEnv = "DEEPSEEK_MODEL_HOME";
  const std::string base = "/tmp/deepseek_models_test";
  SetEnvVar(kEnv, base);

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

  UnsetEnvVar(kEnv);
}

TEST(ModelStoreTests, SanitizesColonsInModelNameForWindowsCompatibility) {
  // Registry-style model tags (e.g. Ollama's "llama3:8b") contain ':',
  // which NTFS treats as alternate-data-stream syntax rather than a
  // normal path character. ResolveModelPath must sanitize it so the
  // resulting path is a real, creatable directory on every platform.
  const char* kEnv = "DEEPSEEK_MODEL_HOME";
  const std::string base = "/tmp/deepseek_models_test_colon";
  SetEnvVar(kEnv, base);

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

  UnsetEnvVar(kEnv);
}
