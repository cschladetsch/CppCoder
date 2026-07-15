#include "cppcoder/ResearchEngine.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// ---------------- FallbackKeywords ----------------

TEST(FallbackKeywordsTest, ExtractsMeaningfulWords) {
    auto kws = cppcoder::FallbackKeywords("How is the encryption key derived for PDF documents?", 5);
    EXPECT_FALSE(kws.empty());
    // "how", "is", "the", "for" are stopwords / too short and should be excluded.
    for (const auto& k : kws) {
        std::string lower = k;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        EXPECT_NE(lower, "how");
        EXPECT_NE(lower, "is");
        EXPECT_NE(lower, "the");
        EXPECT_NE(lower, "for");
    }
}

TEST(FallbackKeywordsTest, RespectsMaxCount) {
    auto kws = cppcoder::FallbackKeywords(
        "encryption decryption parsing tokenizing rendering compiling linking", 3);
    EXPECT_LE(kws.size(), 3u);
}

TEST(FallbackKeywordsTest, EmptyQuestionReturnsEmpty) {
    auto kws = cppcoder::FallbackKeywords("", 5);
    EXPECT_TRUE(kws.empty());
}

TEST(FallbackKeywordsTest, OnlyStopwordsReturnsEmpty) {
    auto kws = cppcoder::FallbackKeywords("how is the a an it", 5);
    EXPECT_TRUE(kws.empty());
}

TEST(FallbackKeywordsTest, ShortWordsUnderThreeCharsExcluded) {
    auto kws = cppcoder::FallbackKeywords("go do it up on ok", 10);
    // All of these are <3 chars or stopwords.
    EXPECT_TRUE(kws.empty());
}

TEST(FallbackKeywordsTest, PreservesOriginalCasing) {
    auto kws = cppcoder::FallbackKeywords("PdfCrypto handles Encryption", 5);
    bool foundOriginalCase = false;
    for (const auto& k : kws) {
        if (k == "PdfCrypto") foundOriginalCase = true;
    }
    EXPECT_TRUE(foundOriginalCase);
}

TEST(FallbackKeywordsTest, UnderscoresKeepIdentifierIntact) {
    auto kws = cppcoder::FallbackKeywords("what does parse_document_stream do", 5);
    bool found = false;
    for (const auto& k : kws) {
        if (k == "parse_document_stream") found = true;
    }
    EXPECT_TRUE(found);
}

// ---------------- SeedInitialTasks ----------------

namespace {

class SeedInitialTasksTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / "cppcoder_engine_test";
        fs::remove_all(root_);
        fs::create_directories(root_);
        std::ofstream(root_ / "crypto.cpp") << "int DeriveKey() { return 0; }\n";
        std::ofstream(root_ / "parser.cpp") << "void ParseDocument() {}\n";
    }
    void TearDown() override { fs::remove_all(root_); }
    fs::path root_;
};

}  // namespace

TEST_F(SeedInitialTasksTest, ReturnsRepeatableSeedTaskWhenKeywordsMatch) {
    cppcoder::OllamaClient client(cppcoder::OllamaConfig{});
    cppcoder::CodebaseScanner scanner(root_);
    cppcoder::ResearchEngine engine(client, scanner, cppcoder::EngineConfig{});

    auto tasks = engine.SeedInitialTasks("How is the key derived?", {"DeriveKey"});
    ASSERT_EQ(tasks.size(), 1u);
    EXPECT_TRUE(tasks[0].repeatable);
    ASSERT_EQ(tasks[0].repeatTargets.size(), 1u);
    EXPECT_EQ(tasks[0].repeatTargets[0], "crypto.cpp");
    EXPECT_EQ(tasks[0].researchGoal, "How is the key derived?");
}

TEST_F(SeedInitialTasksTest, ReturnsEmptyWhenNoKeywordsMatch) {
    cppcoder::OllamaClient client(cppcoder::OllamaConfig{});
    cppcoder::CodebaseScanner scanner(root_);
    cppcoder::ResearchEngine engine(client, scanner, cppcoder::EngineConfig{});

    auto tasks = engine.SeedInitialTasks("question", {"NonexistentSymbolXYZ"});
    EXPECT_TRUE(tasks.empty());
}

TEST_F(SeedInitialTasksTest, DedupesFilesMatchedByMultipleKeywords) {
    cppcoder::OllamaClient client(cppcoder::OllamaConfig{});
    cppcoder::CodebaseScanner scanner(root_);
    cppcoder::ResearchEngine engine(client, scanner, cppcoder::EngineConfig{});

    // Both keywords match crypto.cpp; it should only appear once.
    auto tasks = engine.SeedInitialTasks("q", {"DeriveKey", "int"});
    ASSERT_EQ(tasks.size(), 1u);
    int count = 0;
    for (const auto& t : tasks[0].repeatTargets) {
        if (t == "crypto.cpp") count++;
    }
    EXPECT_EQ(count, 1);
}

TEST_F(SeedInitialTasksTest, MergesMatchesAcrossMultipleKeywords) {
    cppcoder::OllamaClient client(cppcoder::OllamaConfig{});
    cppcoder::CodebaseScanner scanner(root_);
    cppcoder::ResearchEngine engine(client, scanner, cppcoder::EngineConfig{});

    auto tasks = engine.SeedInitialTasks("q", {"DeriveKey", "ParseDocument"});
    ASSERT_EQ(tasks.size(), 1u);
    EXPECT_EQ(tasks[0].repeatTargets.size(), 2u);
}
