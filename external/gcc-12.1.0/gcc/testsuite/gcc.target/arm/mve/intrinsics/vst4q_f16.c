/* { dg-require-effective-target arm_v8_1m_mve_fp_ok } */
/* { dg-add-options arm_v8_1m_mve_fp } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

void
foo (float16_t * addr, float16x8x4_t value)
{
  vst4q_f16 (addr, value);
}

/* { dg-final { scan-assembler "vst40.16"  }  } */
/* { dg-final { scan-assembler "vst41.16"  }  } */
/* { dg-final { scan-assembler "vst42.16"  }  } */
/* { dg-final { scan-assembler "vst43.16"  }  } */

void
foo1 (float16_t * addr, float16x8x4_t value)
{
  vst4q (addr, value);
}

/* { dg-final { scan-assembler "vst40.16"  }  } */
/* { dg-final { scan-assembler "vst41.16"  }  } */
/* { dg-final { scan-assembler "vst42.16"  }  } */
/* { dg-final { scan-assembler "vst43.16"  }  } */

void
foo2 (float16_t * addr, float16x8x4_t value)
{
  vst4q_f16 (addr, value);
  addr += 32;
  vst4q_f16 (addr, value);
}

/* { dg-final { scan-assembler {vst43.16\s\{.*\}, \[.*\]!}  }  } */
