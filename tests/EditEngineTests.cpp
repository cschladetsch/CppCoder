#include "cppcoder/EditEngine.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

class EditEngineSeedTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / "cppcoder_edit_engine_test";
        fs::remove_all(root_);
        fs::create_directories(root_);
        std::ofstream(root_ / "crypto.cpp") << "int DeriveKey() { return 0; }\n";
        std::ofstream(root_ / "parser.cpp") << "void ParseDocument() {}\n";
    }
    void TearDown() override { fs::remove_all(root_); }
    fs::path root_;
};

}  // namespace

TEST_F(EditEngineSeedTest, ReturnsRepeatableSeedTaskWhenKeywordsMatch) {
    cppcoder::OllamaClient client(cppcoder::OllamaConfig{});
    cppcoder::CodebaseScanner scanner(root_);
    cppcoder::EditEngine engine(client, scanner, root_, cppcoder::EditEngineConfig{});

    auto tasks = engine.SeedInitialTasks("Rename DeriveKey to ComputeKey", {"DeriveKey"});
    ASSERT_EQ(tasks.size(), 1u);
    EXPECT_TRUE(tasks[0].repeatable);
    ASSERT_EQ(tasks[0].repeatTargets.size(), 1u);
    EXPECT_EQ(tasks[0].repeatTargets[0], "crypto.cpp");
    EXPECT_EQ(tasks[0].researchGoal, "Rename DeriveKey to ComputeKey");
}

TEST_F(EditEngineSeedTest, ReturnsEmptyWhenNoKeywordsMatch) {
    cppcoder::OllamaClient client(cppcoder::OllamaConfig{});
    cppcoder::CodebaseScanner scanner(root_);
    cppcoder::EditEngine engine(client, scanner, root_, cppcoder::EditEngineConfig{});

    auto tasks = engine.SeedInitialTasks("task", {"NonexistentSymbolXYZ"});
    EXPECT_TRUE(tasks.empty());
}

TEST_F(EditEngineSeedTest, DedupesFilesMatchedByMultipleKeywords) {
    cppcoder::OllamaClient client(cppcoder::OllamaConfig{});
    cppcoder::CodebaseScanner scanner(root_);
    cppcoder::EditEngine engine(client, scanner, root_, cppcoder::EditEngineConfig{});

    // Both keywords match crypto.cpp; it should only appear once.
    auto tasks = engine.SeedInitialTasks("t", {"DeriveKey", "int"});
    ASSERT_EQ(tasks.size(), 1u);
    int count = 0;
    for (const auto& t : tasks[0].repeatTargets) {
        if (t == "crypto.cpp") count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(EditEngineSeedTest, MergesMatchesAcrossMultipleKeywords) {
    cppcoder::OllamaClient client(cppcoder::OllamaConfig{});
    cppcoder::CodebaseScanner scanner(root_);
    cppcoder::EditEngine engine(client, scanner, root_, cppcoder::EditEngineConfig{});

    auto tasks = engine.SeedInitialTasks("t", {"DeriveKey", "ParseDocument"});
    ASSERT_EQ(tasks.size(), 1u);
    EXPECT_EQ(tasks[0].repeatTargets.size(), 2u);
}

TEST_F(EditEngineSeedTest, SeedTaskDefaultsToDryRun) {
    cppcoder::EditEngineConfig config;
    EXPECT_FALSE(config.apply);
}
