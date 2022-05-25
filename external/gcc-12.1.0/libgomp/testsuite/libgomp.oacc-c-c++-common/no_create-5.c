/* Test 'no_create' clause on 'data' construct and nested compute construct,
   with data not present on the device.  */

#include <stdlib.h>
#include <stdio.h>
#include <openacc.h>

#define N 128

int
main (int argc, char *argv[])
{
  int var;
  int *arr = (int *) malloc (N * sizeof (*arr));
  int *devptr[2];

#pragma acc data no_create(var, arr[0:N])
  {
    devptr[0] = (int *) acc_deviceptr (&var);
    devptr[1] = (int *) acc_deviceptr (&arr[2]);

#if ACC_MEM_SHARED
    if (devptr[0] == NULL)
      __builtin_abort ();
    if (devptr[1] == NULL)
      __builtin_abort ();
#else
    if (devptr[0] != NULL)
      __builtin_abort ();
    if (devptr[1] != NULL)
      __builtin_abort ();
#endif

#pragma acc parallel copyout(devptr) // TODO implicit 'copy(var)' -- huh?!
    {
      devptr[0] = &var;
      devptr[1] = &arr[2];
    }

    if (devptr[0] != &var)
      __builtin_abort (); // { dg-xfail-run-if "TODO" { *-*-* } { "-DACC_MEM_SHARED=0" } }
    if (devptr[1] != &arr[2])
      __builtin_abort ();
  }

  free (arr);

  return 0;
}
