/* Test "subset" subarray mappings
   { dg-additional-options "-DOPENACC_RUNTIME" } using OpenACC Runtime Library routines,
   { dg-additional-options "-DARRAYS" } using arrays.  */

/* { dg-skip-if "" { *-*-* } { "-DACC_MEM_SHARED=1" } } */

#include "subset-subarray-mappings-1-r-p.c"
