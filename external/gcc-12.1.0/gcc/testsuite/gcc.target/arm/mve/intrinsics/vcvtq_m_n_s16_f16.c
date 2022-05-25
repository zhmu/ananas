/* { dg-require-effective-target arm_v8_1m_mve_fp_ok } */
/* { dg-add-options arm_v8_1m_mve_fp } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

int16x8_t
foo (int16x8_t inactive, float16x8_t a, mve_pred16_t p)
{
  return vcvtq_m_n_s16_f16 (inactive, a, 1, p);
}

/* { dg-final { scan-assembler "vpst" } } */
/* { dg-final { scan-assembler "vcvtt.s16.f16"  }  } */

int16x8_t
foo1 (int16x8_t inactive, float16x8_t a, mve_pred16_t p)
{
  return vcvtq_m_n (inactive, a, 1, p);
}

/* { dg-final { scan-assembler "vpst" } } */
/* { dg-final { scan-assembler "vcvtt.s16.f16"  }  } */
