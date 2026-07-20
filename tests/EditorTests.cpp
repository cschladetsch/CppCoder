#include "cppcoder/Editor.h"

#include <gtest/gtest.h>

using cppcoder::EditFinding;
using cppcoder::EditOutcome;
using cppcoder::Editor;

TEST(ParseEditResponseTest, SuccessOutcomeWithEditParsed) {
    std::string raw = R"({
        "outcome":"success",
        "summary":"renamed the function",
        "edits":[
            {"path":"src/foo.cpp","new_content":"void Bar() {}\n","description":"renamed Foo to Bar"}
        ]
    })";
    EditFinding f = Editor::ParseEditResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, EditOutcome::Success);
    EXPECT_EQ(f.summary, "renamed the function");
    ASSERT_EQ(f.edits.size(), 1u);
    EXPECT_EQ(f.edits[0].path, "src/foo.cpp");
    EXPECT_EQ(f.edits[0].newContent, "void Bar() {}\n");
    EXPECT_EQ(f.edits[0].description, "renamed Foo to Bar");
    EXPECT_TRUE(f.suggestedDirections.empty());
    EXPECT_EQ(f.areaInvestigated, "src/foo.cpp");
}

TEST(ParseEditResponseTest, NoChangeOutcomeParsed) {
    std::string raw = R"({"outcome":"no_change","summary":"","edits":[],"directions":[]})";
    EditFinding f = Editor::ParseEditResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, EditOutcome::NoChangeNeeded);
    EXPECT_TRUE(f.edits.empty());
}

TEST(ParseEditResponseTest, PartialOutcomeWithEditsAndDirectionsParsed) {
    std::string raw = R"({
        "outcome":"partial",
        "summary":"updated caller",
        "edits":[
            {"path":"src/foo.cpp","new_content":"Bar();\n","description":"updated call site"}
        ],
        "directions":[
            {"target_area":"src/bar.cpp","research_goal":"rename Foo to Bar here too","success_criteria":"no more Foo references"}
        ]
    })";
    EditFinding f = Editor::ParseEditResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, EditOutcome::PartialWithDirections);
    ASSERT_EQ(f.edits.size(), 1u);
    ASSERT_EQ(f.suggestedDirections.size(), 1u);
    EXPECT_EQ(f.suggestedDirections[0].targetArea, "src/bar.cpp");
    EXPECT_EQ(f.suggestedDirections[0].researchGoal, "rename Foo to Bar here too");
    EXPECT_EQ(f.suggestedDirections[0].successCriteria, "no more Foo references");
}

TEST(ParseEditResponseTest, UnknownOutcomeStringDefaultsToNoChangeNeeded) {
    std::string raw = R"({"outcome":"maybe","summary":""})";
    EditFinding f = Editor::ParseEditResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, EditOutcome::NoChangeNeeded);
}

TEST(ParseEditResponseTest, ResponseWrappedInProseAndMarkdownFences) {
    std::string raw = "Sure, here's the change:\n```json\n"
                       R"({"outcome":"success","summary":"ok","edits":[{"path":"a.cpp","new_content":"x"}]})"
                       "\n```\nHope that helps!";
    EditFinding f = Editor::ParseEditResponse(raw, "src/foo.cpp");
    EXPECT_EQ(f.outcome, EditOutcome::Success);
    EXPECT_EQ(f.summary, "ok");
    ASSERT_EQ(f.edits.size(), 1u);
    EXPECT_EQ(f.edits[0].path, "a.cpp");
}

TEST(ParseEditResponseTest, MissingEditsFieldDefaultsToEmpty) {
    std::string raw = R"({"outcome":"success","summary":"x"})";
    EditFinding f = Editor::ParseEditResponse(raw, "area");
    EXPECT_TRUE(f.edits.empty());
}

TEST(ParseEditResponseTest, EditsMissingPathAreFiltered) {
    std::string raw = R"({
        "outcome":"success",
        "edits":[
            {"new_content":"no path given"},
            {"path":"src/valid.cpp","new_content":"valid one"}
        ]
    })";
    EditFinding f = Editor::ParseEditResponse(raw, "area");
    ASSERT_EQ(f.edits.size(), 1u);
    EXPECT_EQ(f.edits[0].path, "src/valid.cpp");
}

TEST(ParseEditResponseTest, DirectionsMissingTargetAreaAreFiltered) {
    std::string raw = R"({
        "outcome":"partial",
        "directions":[
            {"research_goal":"no area given"},
            {"target_area":"src/valid.cpp","research_goal":"valid one"}
        ]
    })";
    EditFinding f = Editor::ParseEditResponse(raw, "area");
    ASSERT_EQ(f.suggestedDirections.size(), 1u);
    EXPECT_EQ(f.suggestedDirections[0].targetArea, "src/valid.cpp");
}

TEST(ParseEditResponseTest, MalformedJsonFallsBackToNoChangeNeeded) {
    std::string raw = R"({"outcome": "success", "edits": )";  // truncated/invalid
    EditFinding f = Editor::ParseEditResponse(raw, "area");
    EXPECT_EQ(f.outcome, EditOutcome::NoChangeNeeded);
}

TEST(ParseEditResponseTest, EmptyRawStringFallsBackToNoChangeNeeded) {
    EditFinding f = Editor::ParseEditResponse("", "area");
    EXPECT_EQ(f.outcome, EditOutcome::NoChangeNeeded);
}

TEST(ParseEditResponseTest, NoJsonAtAllFallsBackToNoChangeNeeded) {
    EditFinding f = Editor::ParseEditResponse("I don't think anything needs to change.", "area");
    EXPECT_EQ(f.outcome, EditOutcome::NoChangeNeeded);
}

TEST(ParseEditResponseTest, MultipleEditsAllParsedWhenValid) {
    std::string raw = R"({
        "outcome":"success",
        "edits":[
            {"path":"a.cpp","new_content":"a"},
            {"path":"b.cpp","new_content":"b"},
            {"path":"c.cpp","new_content":"c"}
        ]
    })";
    EditFinding f = Editor::ParseEditResponse(raw, "area");
    EXPECT_EQ(f.edits.size(), 3u);
}

TEST(ParseEditResponseTest, StripsEchoedScannerHeaderMarkerFromNewContent) {
    // CodebaseScanner prepends "// ==== <path> ====" before each file's
    // content in the prompt; small local models sometimes echo it back
    // as if it were real file content despite being told not to.
    std::string raw = R"({
        "outcome":"success",
        "edits":[
            {"path":"foo.cpp","new_content":"\n// ==== foo.cpp ====\nint Bar() { return 1; }\n"}
        ]
    })";
    EditFinding f = Editor::ParseEditResponse(raw, "foo.cpp");
    ASSERT_EQ(f.edits.size(), 1u);
    EXPECT_EQ(f.edits[0].newContent, "int Bar() { return 1; }\n");
}

TEST(ParseEditResponseTest, ContentWithoutEchoedMarkerIsUnchanged) {
    std::string raw = R"({
        "outcome":"success",
        "edits":[
            {"path":"foo.cpp","new_content":"int Bar() { return 1; }\n"}
        ]
    })";
    EditFinding f = Editor::ParseEditResponse(raw, "foo.cpp");
    ASSERT_EQ(f.edits.size(), 1u);
    EXPECT_EQ(f.edits[0].newContent, "int Bar() { return 1; }\n");
}

TEST(ParseEditResponseTest, GeneratedDirectionIdsAreNonEmptyAndUnique) {
    std::string raw = R"({
        "outcome":"partial",
        "directions":[
            {"target_area":"a.cpp","research_goal":"g1"},
            {"target_area":"b.cpp","research_goal":"g2"}
        ]
    })";
    EditFinding f = Editor::ParseEditResponse(raw, "area");
    ASSERT_EQ(f.suggestedDirections.size(), 2u);
    EXPECT_FALSE(f.suggestedDirections[0].id.empty());
    EXPECT_FALSE(f.suggestedDirections[1].id.empty());
    EXPECT_NE(f.suggestedDirections[0].id, f.suggestedDirections[1].id);
}
