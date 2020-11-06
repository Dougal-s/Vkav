#include <cmath>

#include <gtest/gtest.h>

#include "Calculate.hpp"

TEST(testCalculate, passthrough) {
	EXPECT_FLOAT_EQ(calculate<float>("10"), 10);
	EXPECT_FLOAT_EQ(calculate<float>("4"), 4);
	EXPECT_FLOAT_EQ(calculate<float>("16"), 16);
	EXPECT_FLOAT_EQ(calculate<float>("0"), 0);
	EXPECT_FLOAT_EQ(calculate<float>("17852693"), 17852693);

	EXPECT_FLOAT_EQ(calculate<float>("-10"), -10);
	EXPECT_FLOAT_EQ(calculate<float>("-4"), -4);
	EXPECT_FLOAT_EQ(calculate<float>("-16"), -16);
	EXPECT_FLOAT_EQ(calculate<float>("-0"), -0);
	EXPECT_FLOAT_EQ(calculate<float>("-17852693"), -17852693);
}

TEST(testCalculate, arithmetic) {
	EXPECT_FLOAT_EQ(calculate<float>("1+1"), 2);
	EXPECT_FLOAT_EQ(calculate<float>("2-1"), 1);
	EXPECT_FLOAT_EQ(calculate<float>("6*3"), 18);
	EXPECT_FLOAT_EQ(calculate<float>("6/3-2+1"), 1);
	EXPECT_FLOAT_EQ(calculate<float>("1+5/2-2"), 1.5);

	EXPECT_FLOAT_EQ(calculate<float>("16*3-5/2+3"), 48.5);
	EXPECT_FLOAT_EQ(calculate<float>("-4*-4"), 16);
	EXPECT_FLOAT_EQ(calculate<float>("-2*6 - 12"), -24);
	EXPECT_FLOAT_EQ(calculate<float>("-0 * 12"), 0);
	EXPECT_FLOAT_EQ(calculate<float>("-178524123+27654"), -178496469);

	EXPECT_FLOAT_EQ(calculate<float>("16 % 3"), 1);
	EXPECT_FLOAT_EQ(calculate<float>("2^3"), 8);
	EXPECT_FLOAT_EQ(calculate<float>("2^(5-2)"), 8);
	EXPECT_FLOAT_EQ(calculate<float>("(1+2)*3"), 9);
	EXPECT_FLOAT_EQ(calculate<float>("1+1^12-0"), 2);
}

TEST(testCalculate, functions) {
	EXPECT_FLOAT_EQ(calculate<float>("sin(0)"), 0);
	EXPECT_FLOAT_EQ(calculate<float>("cos(0)"), 1);
	EXPECT_FLOAT_EQ(calculate<float>("tan(0)"), 0);

	EXPECT_FLOAT_EQ(calculate<float>("min(5, 6)"), 5);
	EXPECT_FLOAT_EQ(calculate<float>("min(0, 1)"), 0);
	EXPECT_FLOAT_EQ(calculate<float>("min(1.5, -1.9)"), -1.9f);
	EXPECT_FLOAT_EQ(calculate<float>("min(10*15/7, 6-2)"), 4);
	EXPECT_FLOAT_EQ(calculate<float>("min(3^3, min(6, (0+1))*5)"), 5);

	EXPECT_FLOAT_EQ(calculate<float>("max(2, 6)"), 6);
	EXPECT_FLOAT_EQ(calculate<float>("max(-5, 3)"), 3);
	EXPECT_FLOAT_EQ(calculate<float>("max(1.5, -1.9)"), 1.5);
	EXPECT_FLOAT_EQ(calculate<float>("max(-10*15, 6-2)"), 4);
	EXPECT_FLOAT_EQ(calculate<float>("max(3^3, min(6, max(0, 1^2))*5)"), 27);
}

TEST(testCalculate, constants) {
	EXPECT_FLOAT_EQ(calculate<float>("pi"), M_PI);
	EXPECT_FLOAT_EQ(calculate<float>("cos(pi)"), -1);
	EXPECT_FLOAT_EQ(calculate<float>("e"), std::exp(1));
	EXPECT_FLOAT_EQ(calculate<float>("e^2"), std::exp(2));
}

TEST(testCalculate, whitespace) {
	EXPECT_FLOAT_EQ(calculate<float>("   \n  pi       "), M_PI);
	EXPECT_FLOAT_EQ(calculate<float>("	 cos(        	0.0    )\n  "), 1);
}
