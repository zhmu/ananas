// PR c++/92560
// { dg-do compile { target c++20 } }

#include <compare>

struct X
{
  friend std::strong_ordering operator<=>(X, X);
} x;

using T = decltype(x < x);
