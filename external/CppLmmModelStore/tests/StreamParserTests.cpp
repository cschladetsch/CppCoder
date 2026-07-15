#include "DeepSeekStreamParser.hpp"

#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(StreamParserTests, ParsesDeltasAcrossChunks) {
  std::vector<std::string> reasoning;
  std::vector<std::string> content;

  deepseek::DeepSeekStreamParser parser(
      [&](std::string_view reasoning_delta, std::string_view content_delta) {
        if (!reasoning_delta.empty()) {
          reasoning.emplace_back(reasoning_delta);
        }
        if (!content_delta.empty()) {
          content.emplace_back(content_delta);
        }
      });

  std::string error;
  std::string chunk1 =
      "data: {\"choices\":[{\"delta\":{\"reasoning_content\":\"Think\"}}]}\n"
      "data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}\n";
  ASSERT_TRUE(parser.Feed(chunk1, &error)) << error;

  std::string chunk2_part1 = "data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}";
  std::string chunk2_part2 = "]}\n"
                             "data: [DONE]\n";
  ASSERT_TRUE(parser.Feed(chunk2_part1, &error)) << error;
  ASSERT_TRUE(parser.Feed(chunk2_part2, &error)) << error;

  ASSERT_EQ(reasoning.size(), 1u);
  EXPECT_EQ(reasoning[0], "Think");

  ASSERT_EQ(content.size(), 2u);
  EXPECT_EQ(content[0], "Hel");
  EXPECT_EQ(content[1], "lo");
}
