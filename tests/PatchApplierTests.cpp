#include "cppcoder/PatchApplier.h"
#include "cppcoder/Types.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

class PatchApplierTest : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / "cppcoder_patchapplier_test";
        fs::remove_all(root_);
        fs::create_directories(root_);
        WriteFile(root_ / "existing.cpp", "int Original() { return 1; }\n");
    }

    void TearDown() override { fs::remove_all(root_); }

    static void WriteFile(const fs::path& p, const std::string& content) {
        std::ofstream out(p, std::ios::binary);
        out << content;
    }

    static std::string ReadFile(const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    fs::path root_;
};

}  // namespace

TEST_F(PatchApplierTest, OverwritesExistingFileContent) {
    cppcoder::PatchApplier applier(root_);
    cppcoder::ProposedEdit edit;
    edit.path = "existing.cpp";
    edit.newContent = "int Changed() { return 2; }\n";

    auto outcome = applier.Apply({edit});
    ASSERT_EQ(outcome.writtenPaths.size(), 1u);
    EXPECT_EQ(outcome.writtenPaths[0], "existing.cpp");
    EXPECT_TRUE(outcome.rejectedPaths.empty());
    EXPECT_TRUE(outcome.errors.empty());
    EXPECT_EQ(ReadFile(root_ / "existing.cpp"), "int Changed() { return 2; }\n");
}

TEST_F(PatchApplierTest, CreatesNewFileAndParentDirectories) {
    cppcoder::PatchApplier applier(root_);
    cppcoder::ProposedEdit edit;
    edit.path = "sub/deeper/new_file.cpp";
    edit.newContent = "void NewFunction() {}\n";

    auto outcome = applier.Apply({edit});
    ASSERT_EQ(outcome.writtenPaths.size(), 1u);
    EXPECT_TRUE(fs::exists(root_ / "sub" / "deeper" / "new_file.cpp"));
    EXPECT_EQ(ReadFile(root_ / "sub" / "deeper" / "new_file.cpp"), "void NewFunction() {}\n");
}

TEST_F(PatchApplierTest, RejectsPathTraversalOutsideRoot) {
    cppcoder::PatchApplier applier(root_);
    cppcoder::ProposedEdit edit;
    edit.path = "../outside.cpp";
    edit.newContent = "should never be written";

    auto outcome = applier.Apply({edit});
    EXPECT_TRUE(outcome.writtenPaths.empty());
    ASSERT_EQ(outcome.rejectedPaths.size(), 1u);
    EXPECT_EQ(outcome.rejectedPaths[0], "../outside.cpp");
    EXPECT_FALSE(fs::exists(root_.parent_path() / "outside.cpp"));
}

TEST_F(PatchApplierTest, RejectsDeeperPathTraversalOutsideRoot) {
    cppcoder::PatchApplier applier(root_);
    cppcoder::ProposedEdit edit;
    edit.path = "sub/../../escaped.cpp";
    edit.newContent = "should never be written";

    auto outcome = applier.Apply({edit});
    EXPECT_TRUE(outcome.writtenPaths.empty());
    ASSERT_EQ(outcome.rejectedPaths.size(), 1u);
}

TEST_F(PatchApplierTest, RejectsAbsolutePath) {
    cppcoder::PatchApplier applier(root_);
    cppcoder::ProposedEdit edit;
#ifdef _WIN32
    edit.path = (root_.root_name().string()) + "\\outside_via_absolute.cpp";
#else
    edit.path = "/tmp/outside_via_absolute.cpp";
#endif
    edit.newContent = "should never be written";

    auto outcome = applier.Apply({edit});
    EXPECT_TRUE(outcome.writtenPaths.empty());
    ASSERT_EQ(outcome.rejectedPaths.size(), 1u);
}

TEST_F(PatchApplierTest, RejectsEmptyPath) {
    cppcoder::PatchApplier applier(root_);
    cppcoder::ProposedEdit edit;
    edit.path = "";
    edit.newContent = "irrelevant";

    auto outcome = applier.Apply({edit});
    EXPECT_TRUE(outcome.writtenPaths.empty());
    ASSERT_EQ(outcome.rejectedPaths.size(), 1u);
}

TEST_F(PatchApplierTest, AppliesMultipleEditsInOneCall) {
    cppcoder::PatchApplier applier(root_);
    cppcoder::ProposedEdit a;
    a.path = "a.cpp";
    a.newContent = "// a\n";
    cppcoder::ProposedEdit b;
    b.path = "b.cpp";
    b.newContent = "// b\n";

    auto outcome = applier.Apply({a, b});
    EXPECT_EQ(outcome.writtenPaths.size(), 2u);
    EXPECT_EQ(ReadFile(root_ / "a.cpp"), "// a\n");
    EXPECT_EQ(ReadFile(root_ / "b.cpp"), "// b\n");
}

TEST_F(PatchApplierTest, EmptyEditsListProducesEmptyOutcome) {
    cppcoder::PatchApplier applier(root_);
    auto outcome = applier.Apply({});
    EXPECT_TRUE(outcome.writtenPaths.empty());
    EXPECT_TRUE(outcome.rejectedPaths.empty());
    EXPECT_TRUE(outcome.errors.empty());
}
