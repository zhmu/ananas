#include <gtest/gtest.h>

#define KASSERT(x, ...) EXPECT_TRUE(x) << __VA_ARGS__

#include "../../include/ananas/util/refcounted.h"

TEST(Refcounted, refcounted)
{
    struct Dummy : util::refcounted<Dummy> {
        Dummy(int& d) : destroyed(d) {}

        ~Dummy() { ++destroyed; }

        int& destroyed;
    };

    {
        int destructor_calls{};
        auto dummy = new Dummy(destructor_calls);
        EXPECT_EQ(1, dummy->GetReferenceCount());
        dummy->RemoveReference();
        // dummy is removed here now
        EXPECT_EQ(1, destructor_calls);
    }

    {
        int destructor_calls{};
        auto dummy = new Dummy(destructor_calls);
        EXPECT_EQ(1, dummy->GetReferenceCount());
        dummy->AddReference();
        EXPECT_EQ(2, dummy->GetReferenceCount());
        EXPECT_EQ(0, destructor_calls);
        dummy->AddReference();
        EXPECT_EQ(3, dummy->GetReferenceCount());
        EXPECT_EQ(0, destructor_calls);
        dummy->RemoveReference();
        EXPECT_EQ(2, dummy->GetReferenceCount());
        EXPECT_EQ(0, destructor_calls);
        dummy->RemoveReference();
        EXPECT_EQ(1, dummy->GetReferenceCount());
        EXPECT_EQ(0, destructor_calls);
        dummy->RemoveReference();
        EXPECT_EQ(1, destructor_calls);
    }
}
