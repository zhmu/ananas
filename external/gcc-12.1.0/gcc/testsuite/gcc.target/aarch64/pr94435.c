/* PR target/94435 */
/* { dg-do compile } */
/* { dg-options "-march=armv8-a+nolse -moutline-atomics" } */

int b, c, d, e, f, h;
short g;
int foo (int) __attribute__ ((__const__));

void
bar (void)
{
  while (1)
    {
      while (1)
	{
	  __atomic_load_n (&e, 0);
	  if (foo (2))
	    __sync_val_compare_and_swap (&c, 0, f);
	  b = 1;
	  if (h == e)
	    break;
	}
      __sync_val_compare_and_swap (&g, -1, f);
    }
}
