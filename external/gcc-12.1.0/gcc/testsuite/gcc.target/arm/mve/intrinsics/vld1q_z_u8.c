/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

uint8x16_t
foo (uint8_t const * base, mve_pred16_t p)
{
  return vld1q_z_u8 (base, p);
}

uint8x16_t
foo1 (uint8_t const * base, mve_pred16_t p)
{
  return vld1q_z (base, p);
}

/* { dg-final { scan-assembler-times "vpst" 2 }  } */
/* { dg-final { scan-assembler-times "vldrbt.8" 2 }  } */
/* { dg-final { scan-assembler-not "__ARM_undef" } } */
