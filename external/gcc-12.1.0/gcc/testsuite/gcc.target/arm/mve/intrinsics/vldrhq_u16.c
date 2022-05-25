/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

uint16x8_t
foo (uint16_t const * base)
{
  return vldrhq_u16 (base);
}

/* { dg-final { scan-assembler-times "vldrh.16" 1 }  } */
/* { dg-final { scan-assembler-not "__ARM_undef" } } */
