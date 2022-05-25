/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

int32x4_t
foo (int32x4_t a, int32x4_t b)
{
  return vbicq_s32 (a, b);
}

/* { dg-final { scan-assembler "vbic"  }  } */

int32x4_t
foo1 (int32x4_t a, int32x4_t b)
{
  return vbicq (a, b);
}

/* { dg-final { scan-assembler "vbic"  }  } */
/* { dg-final { scan-assembler-not "__ARM_undef" } } */
