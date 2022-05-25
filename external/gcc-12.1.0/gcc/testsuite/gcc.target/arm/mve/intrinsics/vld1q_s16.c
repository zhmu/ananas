/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

int16x8_t
foo (int16_t const * base)
{
  return vld1q_s16 (base);
}

int16x8_t
foo1 (int16_t const * base)
{
  return vld1q (base);
}

/* { dg-final { scan-assembler-times "vldrh.16" 2 }  } */
/* { dg-final { scan-assembler-not "__ARM_undef" } } */
