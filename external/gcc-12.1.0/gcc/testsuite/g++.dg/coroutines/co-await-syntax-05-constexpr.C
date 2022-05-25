//  { dg-additional-options "-fsyntax-only -w" }

#include "coro.h"

constexpr int bar () {
  co_await coro::suspend_always{}; // { dg-error "cannot be used in a .constexpr. function" }
  return 42; /* Suppress the "no return" error.  */
}

int main () {
  return bar ();
}
