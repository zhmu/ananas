// PR c++/92010
// { dg-do compile { target c++20 } }

template <class T> void spam(decltype([]{}) (*s)[sizeof(T)] = nullptr)
{ }

void foo()
{
  spam<int>();
}
