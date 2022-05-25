/* PR target/95046 */
/* { dg-do compile { target { ! ia32 } } } */
/* { dg-options "-O3 -mavx512vl" } */


float r[2];
int s[2];
unsigned int u[2];

void
test_float (void)
{
  for (int i = 0; i < 2; i++)
    r[i] = s[i];
}

/* { dg-final { scan-assembler "\tvcvtdq2ps" } } */

void
test_ufloat (void)
{
  for (int i = 0; i < 2; i++)
    r[i] = u[i];
}

/* { dg-final { scan-assembler "\tvcvtudq2ps" } } */

void
test_fix (void)
{
  for (int i = 0; i < 2; i++)
    s[i] = r[i];
}

/* { dg-final { scan-assembler "\tvcvttps2dq" } } */

void
test_ufix (void)
{
  for (int i = 0; i < 2; i++)
    u[i] = r[i];
}

/* { dg-final { scan-assembler "\tvcvttps2udq" } } */
