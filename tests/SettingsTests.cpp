#include <gtest/gtest.h>

#include "Settings.hpp"

TEST(testSettings, commandLineArguments) {
	std::array<const char*, 8> args = {
		"execName", "--help", "-a", "10.5",
		"--sinkName=\"name of audio source\"", "-c=\"path/to/config\"",
		"--physicalDevice", "2"
	};
	auto argsParsed = readCmdLineArgs(args.size(), args.data());
	ASSERT_EQ(argsParsed.size(), 5);

	EXPECT_NE(argsParsed.find("help"), argsParsed.end());

	ASSERT_NE(argsParsed.find("amplitude"), argsParsed.end());
	EXPECT_STREQ(argsParsed.find("amplitude")->second.data(), "10.5");

	ASSERT_NE(argsParsed.find("sinkName"), argsParsed.end());
	EXPECT_STREQ(argsParsed.find("sinkName")->second.data(), "\"name of audio source\"");

	ASSERT_NE(argsParsed.find("config"), argsParsed.end());
	EXPECT_STREQ(argsParsed.find("config")->second.data(), "\"path/to/config\"");

	ASSERT_NE(argsParsed.find("physicalDevice"), argsParsed.end());
	EXPECT_STREQ(argsParsed.find("physicalDevice")->second.data(), "2");
}

TEST(testSettings, parseAsString) {
	EXPECT_EQ(parseAsString("\"a string woooo\""), "a string woooo");
	EXPECT_EQ(parseAsString("a string woooo"), "a string woooo");
}

TEST(testSettings, parseAsArray) {
	{
		auto arr = parseAsArray("a string woooo");
		ASSERT_EQ(arr.size(), 1);
		EXPECT_EQ(arr[0], "a string woooo");
	}
	{
		auto arr = parseAsArray("{    a 2 , asd, fds}");
		ASSERT_EQ(arr.size(), 3);
		EXPECT_EQ(arr[0], "a 2");
		EXPECT_EQ(arr[1], "asd");
		EXPECT_EQ(arr[2], "fds");
	}
}

TEST(testSettings, parseAsPair) {
	{
		auto pair = parseAsPair("{ a ,      b  }");
		EXPECT_EQ(pair.first, "a");
		EXPECT_EQ(pair.second, "b");
	}
	{
		auto pair = parseAsPair("{a 2 , asd}");
		EXPECT_EQ(pair.first, "a 2");
		EXPECT_EQ(pair.second, "asd");
	}
}
