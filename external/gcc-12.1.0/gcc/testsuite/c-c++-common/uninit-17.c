/* { dg-do compile } */
/* { dg-options "-O2 -Wuninitialized -fno-ivopts" } */

inline int foo(int x)
{
  return x;
}
static void bar(int a, int *ptr)
{
  do
  {
    int b;
    if (b < 40) {
      ptr[0] = b;
    }
    b += 1; /* { dg-warning "is used uninitialized" } */
    ptr++;
  }
  while (--a != 0);
}
void foobar(int a, int *ptr)
{
  bar(foo(a), ptr);
}

