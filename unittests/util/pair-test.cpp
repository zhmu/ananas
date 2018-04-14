#include <gtest/gtest.h>
#include "../../include/ananas/util/pair.h"

//#include <utility>
//namespace util = std;

TEST(Pair, constructor)
{
	{
		util::pair<int, bool> a;
		EXPECT_EQ(0, a.first);
		EXPECT_EQ(false, a.second);
	}
	{
		util::pair<int, bool> a{42, true};
		EXPECT_EQ(42, a.first);
		EXPECT_EQ(true, a.second);
	}
}

TEST(Pair, make_pair)
{
	{
		auto a = std::make_pair(1, 2);
		EXPECT_EQ(1, a.first);
		EXPECT_EQ(2, a.second);
	}
}

TEST(Pair, conversion)
{
	{
		util::pair<bool, bool> a{true, false};
		util::pair<bool, bool> b{a};
		EXPECT_EQ(1, b.first);
		EXPECT_EQ(0, b.second);
	}
}

TEST(Pair, decomposition)
{
	{
		util::pair<int, bool> a{42, true};
		auto [ u, v ] = a;
		EXPECT_EQ(42, u);
		EXPECT_EQ(true, v);
	}
	{
		auto [ a, b ] = std::make_pair(1, 2);
		EXPECT_EQ(1, a);
		EXPECT_EQ(2, b);
	}
	{
		util::pair<int, int> a{1, 2};
		auto& [ u, v ] = a;
		u = 13;
		v = 37;
		EXPECT_EQ(13, a.first);
		EXPECT_EQ(37, a.second);
	}
}
