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
