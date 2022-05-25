/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

uint16x8_t
foo (uint16x8_t inactive, uint16x8_t a, uint16_t b, mve_pred16_t p)
{
  return vqaddq_m_n_u16 (inactive, a, b, p);
}

/* { dg-final { scan-assembler "vpst" } } */
/* { dg-final { scan-assembler "vqaddt.u16"  }  } */

uint16x8_t
foo1 (uint16x8_t inactive, uint16x8_t a, uint16_t b, mve_pred16_t p)
{
  return vqaddq_m (inactive, a, b, p);
}

/* { dg-final { scan-assembler "vpst" } } */
/* { dg-final { scan-assembler "vqaddt.u16"  }  } */
