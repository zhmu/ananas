/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

uint32x4_t
foo (uint32x4_t inactive, uint32x4_t a, uint32_t b, mve_pred16_t p)
{
  return vaddq_m_n_u32 (inactive, a, b, p);
}

/* { dg-final { scan-assembler "vpst" } } */
/* { dg-final { scan-assembler "vaddt.i32"  }  } */

uint32x4_t
foo1 (uint32x4_t inactive, uint32x4_t a, uint32_t b, mve_pred16_t p)
{
  return vaddq_m (inactive, a, b, p);
}

/* { dg-final { scan-assembler "vpst" } } */
/* { dg-final { scan-assembler "vaddt.i32"  }  } */
