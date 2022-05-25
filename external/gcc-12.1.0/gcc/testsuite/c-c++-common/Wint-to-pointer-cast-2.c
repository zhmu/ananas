/* Test -Wint-to-pointer-cast.  */
/* Origin: Joseph Myers <joseph@codesourcery.com> */
/* { dg-do compile } */
/* { dg-options "-Wint-to-pointer-cast" } */

char c;

void *
f (void)
{
  return (void *) c; /* { dg-warning "10:cast to pointer from integer of different size" } */
}
