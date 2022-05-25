/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

uint16x8_t
foo (uint8_t const * base, mve_pred16_t p)
{
  return vldrbq_z_u16 (base, p);
}

/* { dg-final { scan-assembler "vldrbt.u16"  }  } */
