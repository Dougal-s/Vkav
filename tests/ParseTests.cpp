#include <sstream>

#include <gtest/gtest.h>

#include "ModuleConfig.hpp"

TEST(testParse, basic) {
	std::stringstream stream{
		"vertexCount = 24\n"
		"[resources]\n"
		"(id=4) image normal = \"./normal.jpg\"\n"
		"[parameters]\n"
		"(id=11) int size = 1\n"
		"# position relative to view\n"
		"(id=12) float xPos = 3 # left right\n"
	};

	auto config = parseConfig(stream);

	ASSERT_TRUE(!config.moduleName);

	ASSERT_TRUE(config.vertexCount);
	EXPECT_EQ(config.vertexCount.value(), 24);

	// Resources
	ASSERT_EQ(config.images.size(), 1);

	EXPECT_EQ(config.images[0].id, 4);
	EXPECT_EQ(config.images[0].path, "./normal.jpg");

	// Parameters
	ASSERT_EQ(config.params.size(), 2);

	EXPECT_EQ(config.params[0].id, 11);
	ASSERT_EQ(config.params[0].value.index(), 1);
	EXPECT_EQ(std::get<1>(config.params[0].value), 1);

	EXPECT_EQ(config.params[1].id, 12);
	ASSERT_EQ(config.params[1].value.index(), 2);
	EXPECT_EQ(std::get<2>(config.params[1].value), 3.f);
}
