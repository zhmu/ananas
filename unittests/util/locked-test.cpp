/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <gtest/gtest.h>
#define KASSERT(x, ...) EXPECT_TRUE(x) << __VA_ARGS__
#include "../../include/ananas/util/locked.h"
#include "../../include/ananas/util/utility.h"

struct Widget {
    Widget(int v = {}) : w_value(v) {}

    void Lock() { w_locked = true; }

    void Unlock() { w_locked = false; }

    void AssertLocked() { EXPECT_TRUE(w_locked); }

    void AssertUnlocked() { EXPECT_FALSE(w_locked); }

    int w_value;
    bool w_locked = false;
};

TEST(Locked, constructor) { util::locked<Widget> lw; }

TEST(Locked, basic_usage)
{
    Widget w(123);
    w.Lock();

    {
        util::locked<Widget> lw(w);
        EXPECT_EQ(lw->w_value, 123);
        EXPECT_EQ(&w, &*lw);
        lw.Unlock();
    }

    w.AssertUnlocked();
}

TEST(Locked, extract_object)
{
    Widget w(123);
    w.Lock();

    auto p = [](Widget& w) {
        util::locked<Widget> lw(w);
        return lw.Extract(); // note: must not assert as we give up ownership of w here
    }(w);
    EXPECT_EQ(p, &w);
    EXPECT_TRUE(w.w_locked);
    w.Unlock();
}

TEST(Locked, transfer_object)
{
    Widget w;
    w.Lock();

    util::locked<Widget> lw1(w);
    util::locked<Widget> lw2(util::move(lw1));
    lw2.Unlock();
    w.AssertUnlocked();
}

TEST(Locked, transfer_temporary)
{
    Widget w;
    w.Lock();

    util::locked<Widget> lw;
    lw = util::locked<Widget>(w);
    lw.Unlock();
    w.AssertUnlocked();
}

TEST(Locked, transfer_return)
{
    Widget w;
    w.Lock();

    auto func = [&](Widget& w) {
        util::locked<Widget> lw(w);
        return lw;
    };

    auto lw = func(w);
    lw.Unlock();
    w.AssertUnlocked();
}
