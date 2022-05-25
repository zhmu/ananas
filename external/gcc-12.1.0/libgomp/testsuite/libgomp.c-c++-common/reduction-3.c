/* C / C++'s logical AND and OR operators take any scalar argument
   which compares (un)equal to 0 - the result 1 or 0 and of type int.

   In this testcase, the int result is again converted to a floating-poing
   or complex type.

   While having a floating-point/complex array element with || and && can make
   sense, having a non-integer/non-bool reduction variable is odd but valid.

   Test: integer reduction variable + FP array.  */

#define N 1024
_Complex float rcf[N];
_Complex double rcd[N];
float rf[N];
double rd[N];

int
reduction_or ()
{
  char orf = 0;
  short ord = 0;
  int orfc = 0;
  long ordc = 0;

  #pragma omp parallel reduction(||: orf)
  for (int i=0; i < N; ++i)
    orf = orf || rf[i];

  #pragma omp parallel for reduction(||: ord)
  for (int i=0; i < N; ++i)
    ord = ord || rcd[i];

  #pragma omp parallel for simd reduction(||: orfc)
  for (int i=0; i < N; ++i)
    orfc = orfc || rcf[i];

  #pragma omp parallel loop reduction(||: ordc)
  for (int i=0; i < N; ++i)
    ordc = ordc || rcd[i];

  return orf + ord + __real__ orfc + __real__ ordc;
}

int
reduction_or_teams ()
{
  char orf = 0;
  short ord = 0;
  int orfc = 0;
  long ordc = 0;

  #pragma omp teams distribute parallel for reduction(||: orf)
  for (int i=0; i < N; ++i)
    orf = orf || rf[i];

  #pragma omp teams distribute parallel for simd reduction(||: ord)
  for (int i=0; i < N; ++i)
    ord = ord || rcd[i];

  #pragma omp teams distribute parallel for reduction(||: orfc)
  for (int i=0; i < N; ++i)
    orfc = orfc || rcf[i];

  #pragma omp teams distribute parallel for simd reduction(||: ordc)
  for (int i=0; i < N; ++i)
    ordc = ordc || rcd[i];

  return orf + ord + __real__ orfc + __real__ ordc;
}

int
reduction_and ()
{
  unsigned char andf = 1;
  unsigned short andd = 1;
  unsigned int andfc = 1;
  unsigned long anddc = 1;

  #pragma omp parallel reduction(&&: andf)
  for (int i=0; i < N; ++i)
    andf = andf && rf[i];

  #pragma omp parallel for reduction(&&: andd)
  for (int i=0; i < N; ++i)
    andd = andd && rcd[i];

  #pragma omp parallel for simd reduction(&&: andfc)
  for (int i=0; i < N; ++i)
    andfc = andfc && rcf[i];

  #pragma omp parallel loop reduction(&&: anddc)
  for (int i=0; i < N; ++i)
    anddc = anddc && rcd[i];

  return andf + andd + __real__ andfc + __real__ anddc;
}

int
reduction_and_teams ()
{
  unsigned char andf = 1;
  unsigned short andd = 1;
  unsigned int andfc = 1;
  unsigned long anddc = 1;

  #pragma omp teams distribute parallel for reduction(&&: andf)
  for (int i=0; i < N; ++i)
    andf = andf && rf[i];

  #pragma omp teams distribute parallel for simd reduction(&&: andd)
  for (int i=0; i < N; ++i)
    andd = andd && rcd[i];

  #pragma omp teams distribute parallel for reduction(&&: andfc)
  for (int i=0; i < N; ++i)
    andfc = andfc && rcf[i];

  #pragma omp teams distribute parallel for simd reduction(&&: anddc)
  for (int i=0; i < N; ++i)
    anddc = anddc && rcd[i];

  return andf + andd + __real__ andfc + __real__ anddc;
}

int
main ()
{
  for (int i = 0; i < N; ++i)
    {
      rf[i] = 0;
      rd[i] = 0;
      rcf[i] = 0;
      rcd[i] = 0;
    }

  if (reduction_or () != 0)
    __builtin_abort ();
  if (reduction_or_teams () != 0)
    __builtin_abort ();
  if (reduction_and () != 0)
    __builtin_abort ();
  if (reduction_and_teams () != 0)
    __builtin_abort ();

  rf[10] = 1.0;
  rd[15] = 1.0;
  rcf[10] = 1.0;
  rcd[15] = 1.0i;

  if (reduction_or () != 4)
    __builtin_abort ();
  if (reduction_or_teams () != 4)
    __builtin_abort ();
  if (reduction_and () != 0)
    __builtin_abort ();
  if (reduction_and_teams () != 0)
    __builtin_abort ();

  for (int i = 0; i < N; ++i)
    {
      rf[i] = 1;
      rd[i] = 1;
      rcf[i] = 1;
      rcd[i] = 1;
    }

  if (reduction_or () != 4)
    __builtin_abort ();
  if (reduction_or_teams () != 4)
    __builtin_abort ();
  if (reduction_and () != 4)
    __builtin_abort ();
  if (reduction_and_teams () != 4)
    __builtin_abort ();

  rf[10] = 0.0;
  rd[15] = 0.0;
  rcf[10] = 0.0;
  rcd[15] = 0.0;

  if (reduction_or () != 4)
    __builtin_abort ();
  if (reduction_or_teams () != 4)
    __builtin_abort ();
  if (reduction_and () != 0)
    __builtin_abort ();
  if (reduction_and_teams () != 0)
    __builtin_abort ();

  return 0;
}
