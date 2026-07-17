#include "cppcoder/FactExtractor.h"

#include <gtest/gtest.h>

using cppcoder::ExtractFacts;

TEST(FactExtractorTest, ExtractsNameWithIs) {
    auto facts = ExtractFacts("My name is Christian, nice to meet you.");
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0], "The user's name is Christian.");
}

TEST(FactExtractorTest, ExtractsNameWithoutIs) {
    auto facts = ExtractFacts("My name Christian");
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0], "The user's name is Christian.");
}

TEST(FactExtractorTest, ExtractsAssistantName) {
    auto facts = ExtractFacts("Your name is Charlie from now on.");
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0], "The assistant should be called Charlie.");
}

TEST(FactExtractorTest, ExtractsAgeWithYoSuffix) {
    auto facts = ExtractFacts("I am 55yo and I like C++.");
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0], "The user is 55 years old.");
}

TEST(FactExtractorTest, ExtractsAgeWithYearsOld) {
    auto facts = ExtractFacts("I'm 42 years old.");
    ASSERT_EQ(facts.size(), 1u);
    EXPECT_EQ(facts[0], "The user is 42 years old.");
}

TEST(FactExtractorTest, ExtractsMultipleFactsFromOneMessage) {
    auto facts = ExtractFacts("My name is Christian and I am 55yo.");
    ASSERT_EQ(facts.size(), 2u);
    EXPECT_EQ(facts[0], "The user's name is Christian.");
    EXPECT_EQ(facts[1], "The user is 55 years old.");
}

TEST(FactExtractorTest, PlainQuestionExtractsNothing) {
    auto facts = ExtractFacts("How does the judge prune directions?");
    EXPECT_TRUE(facts.empty());
}

TEST(FactExtractorTest, EmptyMessageExtractsNothing) {
    auto facts = ExtractFacts("");
    EXPECT_TRUE(facts.empty());
}
