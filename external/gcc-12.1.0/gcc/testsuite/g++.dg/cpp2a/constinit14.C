// PR c++/92134 - constinit malfunction in static data member.
// { dg-do compile { target c++20 } }

struct Value {
  Value() : v{new int{42}} {}	// { dg-error "result of 'operator new'" "" { target implicit_constexpr } }
  int* v;
};

struct S {
  static constinit inline Value v{}; // { dg-error "variable .S::v. does not have a constant initializer|call to non-.constexpr. function" }
};

int main() { return *S::v.v; }
