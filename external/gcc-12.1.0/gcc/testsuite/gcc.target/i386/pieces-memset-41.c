/* { dg-do compile } */
/* { dg-options "-O2 -mno-avx2 -mavx -mtune=sandybridge -mno-stackrealign" } */

extern char *dst;

void
foo (int x)
{
  __builtin_memset (dst, x, 33);
}

/* { dg-final { scan-assembler-times "vmovdqu\[ \\t\]+\[^\n\]*%xmm" 2 } } */
/* No need to dynamically realign the stack here.  */
/* { dg-final { scan-assembler-not "and\[^\n\r]*%\[re\]sp" } } */
/* Nor use a frame pointer.  */
/* { dg-final { scan-assembler-not "%\[re\]bp" } } */
