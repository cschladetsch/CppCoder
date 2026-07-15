#include "cppcoder/JsonUtil.h"

#include <gtest/gtest.h>

using cppcoder::ExtractJsonArray;
using cppcoder::ExtractJsonObject;

// ---------------- ExtractJsonObject ----------------

TEST(ExtractJsonObjectTest, PlainObject) {
    EXPECT_EQ(ExtractJsonObject(R"({"a":1})"), R"({"a":1})");
}

TEST(ExtractJsonObjectTest, ObjectWithLeadingProse) {
    EXPECT_EQ(ExtractJsonObject(R"(Sure, here you go: {"a":1})"), R"({"a":1})");
}

TEST(ExtractJsonObjectTest, ObjectWithTrailingProse) {
    EXPECT_EQ(ExtractJsonObject(R"({"a":1} hope that helps!)"), R"({"a":1})");
}

TEST(ExtractJsonObjectTest, ObjectWrappedInMarkdownFence) {
    EXPECT_EQ(ExtractJsonObject("```json\n{\"a\":1}\n```"), R"({"a":1})");
}

TEST(ExtractJsonObjectTest, NestedObjectKeepsOutermostBraces) {
    std::string nested = R"({"a":{"b":2}})";
    EXPECT_EQ(ExtractJsonObject(nested), nested);
}

TEST(ExtractJsonObjectTest, NoBracesReturnsEmpty) {
    EXPECT_EQ(ExtractJsonObject("no json here"), "");
}

TEST(ExtractJsonObjectTest, EmptyStringReturnsEmpty) {
    EXPECT_EQ(ExtractJsonObject(""), "");
}

TEST(ExtractJsonObjectTest, OnlyClosingBraceReturnsEmpty) {
    EXPECT_EQ(ExtractJsonObject("no open brace here }"), "");
}

TEST(ExtractJsonObjectTest, OnlyOpeningBraceReturnsEmpty) {
    EXPECT_EQ(ExtractJsonObject("{ no close brace"), "");
}

TEST(ExtractJsonObjectTest, MultipleObjectsPicksOutermostSpan) {
    // first '{' to last '}' -- this is a known, documented limitation
    // (not a real parser), so two sibling objects merge into one span.
    std::string input = R"({"a":1} and also {"b":2})";
    EXPECT_EQ(ExtractJsonObject(input), R"({"a":1} and also {"b":2})");
}

// ---------------- ExtractJsonArray ----------------

TEST(ExtractJsonArrayTest, PlainArray) {
    EXPECT_EQ(ExtractJsonArray(R"(["a","b"])"), R"(["a","b"])");
}

TEST(ExtractJsonArrayTest, ArrayWithSurroundingProse) {
    EXPECT_EQ(ExtractJsonArray(R"(Keywords: ["a","b"] -- done)"), R"(["a","b"])");
}

TEST(ExtractJsonArrayTest, EmptyArray) {
    EXPECT_EQ(ExtractJsonArray("[]"), "[]");
}

TEST(ExtractJsonArrayTest, NoBracketsReturnsEmpty) {
    EXPECT_EQ(ExtractJsonArray("nothing to see"), "");
}

TEST(ExtractJsonArrayTest, EmptyStringReturnsEmpty) {
    EXPECT_EQ(ExtractJsonArray(""), "");
}

TEST(ExtractJsonArrayTest, OnlyOpenBracketReturnsEmpty) {
    EXPECT_EQ(ExtractJsonArray("[ unterminated"), "");
}
