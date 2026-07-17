#include "cppcoder/MemoryStore.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using cppcoder::MemoryStore;

namespace {

// setenv/unsetenv are POSIX-only; MSVC's CRT doesn't provide them.
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

class MemoryStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = (fs::temp_directory_path() / "cppcoder_memory_test" / "memory.json").string();
        std::error_code ec;
        fs::remove_all(fs::path(path_).parent_path(), ec);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(fs::path(path_).parent_path(), ec);
    }

    std::string path_;
};

}  // namespace

TEST_F(MemoryStoreTest, StartsEmptyWhenNoFileExists) {
    MemoryStore store(path_);
    EXPECT_TRUE(store.AllFacts().empty());
}

TEST_F(MemoryStoreTest, AddFactPersistsAndIsRetrievable) {
    MemoryStore store(path_);
    EXPECT_TRUE(store.AddFact("The user's name is Christian."));
    ASSERT_EQ(store.AllFacts().size(), 1u);
    EXPECT_EQ(store.AllFacts()[0], "The user's name is Christian.");
    EXPECT_TRUE(fs::exists(path_));
}

TEST_F(MemoryStoreTest, AddFactRejectsEmptyOrWhitespace) {
    MemoryStore store(path_);
    EXPECT_FALSE(store.AddFact(""));
    EXPECT_FALSE(store.AddFact("   "));
    EXPECT_TRUE(store.AllFacts().empty());
}

TEST_F(MemoryStoreTest, AddFactDedupesCaseInsensitively) {
    MemoryStore store(path_);
    EXPECT_TRUE(store.AddFact("The user is 55 years old."));
    EXPECT_FALSE(store.AddFact("the user is 55 years old."));
    EXPECT_FALSE(store.AddFact("THE USER IS 55 YEARS OLD."));
    EXPECT_EQ(store.AllFacts().size(), 1u);
}

TEST_F(MemoryStoreTest, RemoveFactDeletesMatchingEntry) {
    MemoryStore store(path_);
    store.AddFact("Fact one.");
    store.AddFact("Fact two.");
    EXPECT_TRUE(store.RemoveFact("fact one."));
    ASSERT_EQ(store.AllFacts().size(), 1u);
    EXPECT_EQ(store.AllFacts()[0], "Fact two.");
}

TEST_F(MemoryStoreTest, RemoveFactReturnsFalseWhenNotFound) {
    MemoryStore store(path_);
    store.AddFact("Fact one.");
    EXPECT_FALSE(store.RemoveFact("nonexistent fact"));
    EXPECT_EQ(store.AllFacts().size(), 1u);
}

TEST_F(MemoryStoreTest, FactsPersistAcrossInstances) {
    {
        MemoryStore store(path_);
        store.AddFact("The user's name is Christian.");
        store.AddFact("The user is 55 years old.");
    }
    MemoryStore reopened(path_);
    ASSERT_EQ(reopened.AllFacts().size(), 2u);
    EXPECT_EQ(reopened.AllFacts()[0], "The user's name is Christian.");
    EXPECT_EQ(reopened.AllFacts()[1], "The user is 55 years old.");
}

TEST(MemoryStoreDefaultPathTest, PrefersExplicitOverrideEnvVar) {
    const std::string override_path =
        (fs::temp_directory_path() / "cppcoder_memory_override.json").string();
    SetEnvVar("CPPCODER_MEMORY_FILE", override_path);
    EXPECT_EQ(MemoryStore::ResolveDefaultPath(), override_path);
    UnsetEnvVar("CPPCODER_MEMORY_FILE");
}

TEST(MemoryStoreDefaultPathTest, FallsBackToModelHomeWhenSet) {
    UnsetEnvVar("CPPCODER_MEMORY_FILE");
    const std::string model_home = (fs::temp_directory_path() / "cppcoder_model_home").string();
    SetEnvVar("DEEPSEEK_MODEL_HOME", model_home);
    EXPECT_EQ(MemoryStore::ResolveDefaultPath(), (fs::path(model_home) / "memory.json").string());
    UnsetEnvVar("DEEPSEEK_MODEL_HOME");
}
