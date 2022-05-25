// PR c++/94074 - wrong modifying const object error for COMPONENT_REF.
// { dg-do compile { target c++14 } }

typedef decltype (sizeof (0)) size_t;

template <typename E, size_t N>
struct array
{
  constexpr const E &operator[](size_t n) const noexcept { return elems[n]; }
  E elems[N];
};

template <typename E, size_t N>
struct array2 {
  array<E, N> a;
};

template <typename T>
struct S {
  using U = array2<T, 4>;
  U m;
  constexpr S(int) : m{}
  {
    const_cast<int &>(const_cast<const U &>(m).a[0]) = 42;
  }
};

constexpr S<int> p = { 10 };
