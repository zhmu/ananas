#include <stdlib.h>

static void calls_free(int *q)
{
  free(q);
}

void test(void *p)
{
  calls_free(p);

  free(p); /* { dg-warning "double-'free' of 'p'" } */
}
