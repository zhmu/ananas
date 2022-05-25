/* { dg-do compile } */
/* { dg-additional-options "-Wuninitialized" } */

void
foo (void)
{
  int i;

#pragma acc parallel
  {
    i = 1;
  }
}


void
foo2 (void)
{
  int i;
  /* { dg-note {'i' was declared here} {} { target *-*-* } .-1 } */

#pragma acc parallel firstprivate (i) /* { dg-warning "is used uninitialized" } */
  {
    i = 1;
  }
}
