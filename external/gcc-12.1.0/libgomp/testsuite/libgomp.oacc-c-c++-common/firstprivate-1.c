/* { dg-additional-options "-fopt-info-note-omp" }
   { dg-additional-options "--param=openacc-privatization=noisy" }
   { dg-additional-options "-foffload=-fopt-info-note-omp" }
   { dg-additional-options "-foffload=--param=openacc-privatization=noisy" }
   for testing/documenting aspects of that functionality.  */

/* { dg-additional-options "-Wopenacc-parallelism" } for testing/documenting
   aspects of that functionality.  */

#include  <openacc.h>


void t1 ()
{
  int ok = 1;
  int val = 2;
  int ary[32];
  int ondev = 0;

  for (int i = 0; i < 32; i++)
    ary[i] = ~0;
  
#pragma acc parallel num_gangs (32) copy (ok) firstprivate (val) copy(ary, ondev)
  /* { dg-note {variable 'i' declared in block isn't candidate for adjusting OpenACC privatization level: not addressable} "" { target *-*-* } .-1 } */
  {
    ondev = acc_on_device (acc_device_not_host);
#pragma acc loop gang(static:1)
    /* { dg-note {variable 'i' in 'private' clause isn't candidate for adjusting OpenACC privatization level: not addressable} "" { target *-*-* } .-1 } */
    for (unsigned i = 0; i < 32; i++)
      {
	if (val != 2)
	  ok = 0;
	val += i;
	ary[i] = val;
      }
  }

  if (ondev)
    {
      if (!ok)
	__builtin_abort ();
      if (val != 2)
	__builtin_abort ();

      for (int i = 0; i < 32; i++)
	if (ary[i] != 2 + i)
	  __builtin_abort ();
    }
}


void t2 ()
{
  int ok = 1;
  int val = 2;

#pragma acc data copy(val)
  {
#pragma acc parallel present (val)
    {
      val = 7;
    }

#pragma acc parallel firstprivate (val) copy(ok)
    {
      ok  = val == 7;
      val = 9;
    }
  }

  if (!ok)
    __builtin_abort ();
  if (val != 7)
    __builtin_abort ();
}


#define N 100
void t3 ()
{
  int a, b[N], c, d, i;
  int n = acc_get_device_type () != acc_device_host ? N : 1;

  a = 5;
  for (i = 0; i < n; i++)
    b[i] = -1;

  #pragma acc parallel num_gangs (n) firstprivate (a)
  #pragma acc loop gang
  /* { dg-note {variable 'i' in 'private' clause isn't candidate for adjusting OpenACC privatization level: not addressable} "" { target *-*-* } .-1 } */
  for (i = 0; i < n; i++)
    {
      a = a + i;
      b[i] = a;
    }

  for (i = 0; i < n; i++)
    if (a + i != b[i])
      __builtin_abort ();

  #pragma acc data copy (a)
  {
    #pragma acc parallel firstprivate (a) copyout (c)
    {
      a = 10;
      c = a;
    }

    /* This version of 'a' should still be 5.  */
    #pragma acc parallel copyout (d) present (a)
    {
      d = a;
    }
  }

  if (c != 10)
    __builtin_abort ();
  if (d != 5)
    __builtin_abort ();
}
#undef N


void t4 ()
{
  int x = 5, i, arr[32];

  for (i = 0; i < 32; i++)
    arr[i] = 3;

#pragma acc parallel firstprivate(x) copy(arr) num_gangs(32) num_workers(8) vector_length(32)
  /* { dg-warning "region is worker partitioned but does not contain worker partitioned code" "" { target *-*-* } .-1 } */
  /* { dg-warning "region is vector partitioned but does not contain vector partitioned code" "" { target *-*-* } .-2 } */
  {
#pragma acc loop gang
    /* { dg-note {variable 'i' in 'private' clause isn't candidate for adjusting OpenACC privatization level: not addressable} "" { target *-*-* } .-1 } */
    for (i = 0; i < 32; i++)
      arr[i] += x;
  }

  for (i = 0; i < 32; i++)
    if (arr[i] != 8)
      __builtin_abort ();
}


int
main()
{
  t1 ();
  t2 ();
  t3 ();
  t4 ();

  return 0;
}
