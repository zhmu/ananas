/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <gtest/gtest.h>
#include "../../include/ananas/util/list.h"

struct Entry : util::List<Entry>::NodePtr {
    Entry(int v) : value(v) {}
    int value;
};

TEST(List, empty)
{
    util::List<int> l;
    EXPECT_TRUE(l.empty());
}

TEST(List, push_back)
{
    Entry e1(1), e2(2), e3(3);

    util::List<Entry> l;
    l.push_back(e1);
    EXPECT_FALSE(l.empty());
    l.push_back(e2);
    EXPECT_FALSE(l.empty());
    l.push_back(e3);
    EXPECT_FALSE(l.empty());

    EXPECT_EQ(1, l.front().value);
    EXPECT_EQ(3, l.back().value);

    int n = 0;
    for (auto& v : l) {
        n++;
        EXPECT_EQ(n, v.value);
    }
    EXPECT_EQ(3, n);
}

TEST(List, push_front)
{
    Entry e1(1), e2(2), e3(3);
    util::List<Entry> l;
    l.push_front(e1);
    EXPECT_FALSE(l.empty());
    l.push_front(e2);
    EXPECT_FALSE(l.empty());
    l.push_front(e3);
    EXPECT_FALSE(l.empty());

    EXPECT_EQ(3, l.front().value);
    EXPECT_EQ(1, l.back().value);

    int n = 3;
    for (auto& v : l) {
        EXPECT_EQ(n, v.value);
        n--;
    }
    EXPECT_EQ(0, n);
}

TEST(List, remove)
{
    {
        Entry e1(1);
        util::List<Entry> l;
        l.push_back(e1);
        l.remove(e1);
        EXPECT_TRUE(l.empty());
    }

    {
        Entry e1(1), e2(2), e3(3);
        util::List<Entry> l;
        l.push_back(e1);
        l.push_back(e2);
        l.push_back(e3);
        l.remove(e2);
        EXPECT_FALSE(l.empty());
        EXPECT_EQ(1, l.front().value);
        EXPECT_EQ(3, l.back().value);
        l.remove(e1);
        EXPECT_FALSE(l.empty());
        EXPECT_EQ(3, l.front().value);
        EXPECT_EQ(3, l.back().value);
        l.remove(e3);
        EXPECT_TRUE(l.empty());
    }
}

TEST(List, iterator)
{
    Entry e1(1), e2(2), e3(3);
    util::List<Entry> l;
    l.push_back(e1);
    l.push_back(e2);
    l.push_back(e3);

    auto it = l.begin();
    EXPECT_EQ(1, it->value);
    ++it;
    EXPECT_EQ(2, it->value);
    ++it;
    EXPECT_EQ(3, it->value);
    ++it;
    EXPECT_EQ(l.end(), it);
    --it;
    EXPECT_EQ(3, it->value);
    --it;
    EXPECT_EQ(2, it->value);
    --it;
    EXPECT_EQ(1, it->value);
    EXPECT_EQ(l.begin(), it);

    EXPECT_EQ(1, (it++)->value);
    EXPECT_EQ(2, (it++)->value);
    EXPECT_EQ(3, (it++)->value);
    EXPECT_EQ(l.end(), it);

    --it;
    EXPECT_EQ(3, (it--)->value);
    EXPECT_EQ(2, (it--)->value);
    EXPECT_EQ(1, it->value);
    EXPECT_EQ(l.begin(), it);
}

#if 0
TEST(List, const_iterator)
{
	Entry e1(1), e2(2), e3(3);
	util::List<Entry> l;
	l.push_back(e1);
	l.push_back(e2);
	l.push_back(e3);

	auto it = l.cbegin();
	EXPECT_EQ(1, it->value); ++it;
	EXPECT_EQ(2, it->value); ++it;
	EXPECT_EQ(3, it->value); ++it;
	EXPECT_EQ(l.end(), it);
	--it;
	EXPECT_EQ(3, it->value); --it;
	EXPECT_EQ(2, it->value); --it;
	EXPECT_EQ(1, it->value);
	EXPECT_EQ(l.begin(), it);

	EXPECT_EQ(1, (it++)->value);
	EXPECT_EQ(2, (it++)->value);
	EXPECT_EQ(3, (it++)->value);
	EXPECT_EQ(l.end(), it);

	--it;
	EXPECT_EQ(3, (it--)->value);
	EXPECT_EQ(2, (it--)->value);
	EXPECT_EQ(1, it->value);
	EXPECT_EQ(l.begin(), it);
}
#endif

TEST(List, iterator_empty_list)
{
    util::List<Entry> l;
    EXPECT_EQ(l.begin(), l.end());
    EXPECT_EQ(l.rbegin(), l.rend());
}

TEST(List, reverse_iterator)
{
    Entry e1(1), e2(2), e3(3);
    util::List<Entry> l;
    l.push_back(e1);
    l.push_back(e2);
    l.push_back(e3);

    auto rit = l.rbegin();
    EXPECT_EQ(3, rit->value);
    ++rit;
    EXPECT_EQ(2, rit->value);
    ++rit;
    EXPECT_EQ(1, rit->value);
    ++rit;
    EXPECT_EQ(l.rend(), rit);

    --rit;
    EXPECT_EQ(1, rit->value);
    --rit;
    EXPECT_EQ(2, rit->value);
    --rit;
    EXPECT_EQ(3, rit->value);
    EXPECT_EQ(l.rbegin(), rit);

    EXPECT_EQ(3, (rit++)->value);
    EXPECT_EQ(2, (rit++)->value);
    EXPECT_EQ(1, (rit++)->value);
    EXPECT_EQ(l.rend(), rit);
}

TEST(List, insert)
{
    Entry e1(1), e2(2), e3(3);

    util::List<Entry> l;
    l.push_back(e3);
    l.insert(e3, e1);
    {
        auto it = l.begin();
        EXPECT_EQ(1, it->value);
        ++it;
        EXPECT_EQ(3, it->value);
        ++it;
        EXPECT_EQ(l.end(), it);
    }
    l.insert(e3, e2);
    {
        auto it = l.begin();
        EXPECT_EQ(1, it->value);
        ++it;
        EXPECT_EQ(2, it->value);
        ++it;
        EXPECT_EQ(3, it->value);
        ++it;
        EXPECT_EQ(l.end(), it);
    }
}

template<typename T>
struct GetAllNodes {
    static typename util::List<T>::Node& Get(T& t) { return t.e_AllNodes; }
};

template<typename T>
struct GetChildNodes {
    static typename util::List<T>::Node& Get(T& t) { return t.e_ChildNodes; }
};

template<typename T>
using NodesAccessor = typename util::List<T>::template nodeptr_accessor<GetAllNodes<T>>;
template<typename T>
using ChildAccessor = typename util::List<T>::template nodeptr_accessor<GetChildNodes<T>>;

TEST(List, node_in_multiple_lists)
{
    struct Node;
    typedef util::List<Node, ChildAccessor<Node>> ChildrenList;
    typedef util::List<Node, NodesAccessor<Node>> NodeList;

    struct Node {
        Node(int v) : e_value(v) {}

        int e_value;

        util::List<Node>::Node e_AllNodes;
        util::List<Node>::Node e_ChildNodes;

        ChildrenList e_Children;
    };

    Node n1(1), n2(2), n3(3), n4(4), n5(5), n6(6);

    NodeList nodes;

    nodes.push_back(n1);
    nodes.push_back(n2);
    nodes.push_back(n3);
    nodes.push_back(n4);
    nodes.push_back(n5);
    nodes.push_back(n6);

    n1.e_Children.push_back(n2);
    n1.e_Children.push_back(n3);
    n2.e_Children.push_back(n4);
    n4.e_Children.push_back(n5);

    // Everything is in the nodes list
    {
        auto it = nodes.begin();
        EXPECT_EQ(1, it->e_value);
        ++it;
        EXPECT_EQ(2, it->e_value);
        ++it;
        EXPECT_EQ(3, it->e_value);
        ++it;
        EXPECT_EQ(4, it->e_value);
        ++it;
        EXPECT_EQ(5, it->e_value);
        ++it;
        EXPECT_EQ(6, it->e_value);
        ++it;
        EXPECT_EQ(nodes.end(), it);
    }

    // Node 1 has children 2 and 3
    {
        int n = 2;
        for (auto& node : n1.e_Children) {
            EXPECT_EQ(n, node.e_value);
            ++n;
        }
        EXPECT_EQ(4, n);
    }

    // Nodes 2 has child 4
    {
        int n = 4;
        for (auto& node : n2.e_Children) {
            EXPECT_EQ(n, node.e_value);
            ++n;
        }
        EXPECT_EQ(5, n);
    }

    // Nodes 3, 5 and 6 have no children
    EXPECT_EQ(n3.e_Children.end(), n3.e_Children.begin());
    EXPECT_EQ(n5.e_Children.end(), n5.e_Children.begin());
    EXPECT_EQ(n6.e_Children.end(), n6.e_Children.begin());
}
