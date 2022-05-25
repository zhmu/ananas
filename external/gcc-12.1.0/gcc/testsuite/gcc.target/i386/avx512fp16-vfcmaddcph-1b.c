/* { dg-do run { target avx512fp16 } } */
/* { dg-options "-O2 -mavx512fp16 -mavx512dq" } */


#define AVX512FP16
#include "avx512fp16-helper.h"

#define N_ELEMS (AVX512F_LEN / 16)

void NOINLINE
EMULATE(c_fmadd_pch) (V512 * dest, V512 op1, V512 op2,
		    __mmask16 k, int zero_mask, int c_flag,
		    int is_mask3)
{
  V512 v1, v2, v3, v4, v5, v6, v7, v8;
  int i;
  int invert = 1;
  if (c_flag == 1)
    invert = -1;

  unpack_ph_2twops(op1, &v1, &v2);
  unpack_ph_2twops(op2, &v3, &v4);
  unpack_ph_2twops(*dest, &v7, &v8);

  for (i = 0; i < 16; i++) {
    if (((1 << (i / 2)) & k) == 0) {
      if (zero_mask) {
	v5.f32[i] = 0;
      }
      else {
	v5.u32[i] = is_mask3 ? v3.u32[i] : v7.u32[i];
      }
    }
    else {
      if ((i % 2) == 0) {
	v5.f32[i] = v1.f32[i] * v7.f32[i]
	  - invert * (v1.f32[i+1] * v7.f32[i+1]) + v3.f32[i];
      }
      else {
	v5.f32[i] = v1.f32[i-1] * v7.f32[i]
	  + invert * (v1.f32[i] * v7.f32[i-1]) + v3.f32[i];

      }
    }
    if (((1 << (i / 2 + 8)) & k) == 0) {
      if (zero_mask) {
	v6.f32[i] = 0;
      }
      else {
	v6.u32[i] = is_mask3 ? v4.u32[i] : v8.u32[i];
      }
    }
    else {
      if ((i % 2) == 0) {
	v6.f32[i] = v2.f32[i] * v8.f32[i]
	  - invert * (v2.f32[i+1] * v8.f32[i+1]) + v4.f32[i];
      }
      else {
	v6.f32[i] = v2.f32[i-1] * v8.f32[i]
	  + invert * (v2.f32[i] * v8.f32[i-1]) + v4.f32[i];
      }

    }
  }

  *dest = pack_twops_2ph(v5, v6);
}

void
TEST (void)
{
  V512 res;
  V512 exp;

  init_src();

  init_dest(&res, &exp);
  EMULATE(c_fmadd_pch)(&exp, src1, src2, NET_CMASK, 0, 1, 0);
  HF(res) = INTRINSIC (_fcmadd_pch) (HF(res), HF(src1),
				     HF(src2));
  CHECK_RESULT (&res, &exp, N_ELEMS, _fcmadd_pch);

  init_dest(&res, &exp);
  EMULATE(c_fmadd_pch)(&exp, src1, src2, HALF_MASK, 0, 1, 0);
  HF(res) = INTRINSIC (_mask_fcmadd_pch) (HF(res) ,HALF_MASK, HF(src1),
					  HF(src2));
  CHECK_RESULT (&res, &exp, N_ELEMS, _mask_fcmadd_pch);

  init_dest(&res, &exp);
  EMULATE(c_fmadd_pch)(&exp, src1, src2, HALF_MASK, 0, 1, 1);
  HF(res) = INTRINSIC (_mask3_fcmadd_pch) (HF(res), HF(src1),
					   HF(src2), HALF_MASK);
  CHECK_RESULT (&res, &exp, N_ELEMS, _mask3_fcmadd_pch);


  init_dest(&res, &exp);
  EMULATE(c_fmadd_pch)(&exp, src1, src2, HALF_MASK, 1, 1, 0);
  HF(res) = INTRINSIC (_maskz_fcmadd_pch) (HALF_MASK, HF(res),
					   HF(src1), HF(src2));
  CHECK_RESULT (&res, &exp, N_ELEMS, _maskz_fcmadd_pch);

#if AVX512F_LEN == 512
  init_dest(&res, &exp);
  EMULATE(c_fmadd_pch)(&exp, src1, src2, NET_CMASK, 0, 1, 0);
  HF(res) = INTRINSIC (_fcmadd_round_pch) (HF(res), HF(src1),
				     HF(src2), _ROUND_NINT);
  CHECK_RESULT (&res, &exp, N_ELEMS, _fcmadd_pch);

  init_dest(&res, &exp);
  EMULATE(c_fmadd_pch)(&exp, src1, src2, HALF_MASK, 0, 1, 0);
  HF(res) = INTRINSIC (_mask_fcmadd_round_pch) (HF(res) ,HALF_MASK, HF(src1),
					  HF(src2), _ROUND_NINT);
  CHECK_RESULT (&res, &exp, N_ELEMS, _mask_fcmadd_pch);

  init_dest(&res, &exp);
  EMULATE(c_fmadd_pch)(&exp, src1, src2, HALF_MASK, 0, 1, 1);
  HF(res) = INTRINSIC (_mask3_fcmadd_round_pch) (HF(res), HF(src1),
					   HF(src2), HALF_MASK, _ROUND_NINT);
  CHECK_RESULT (&res, &exp, N_ELEMS, _mask3_fcmadd_pch);


  init_dest(&res, &exp);
  EMULATE(c_fmadd_pch)(&exp, src1, src2, HALF_MASK, 1, 1, 0);
  HF(res) = INTRINSIC (_maskz_fcmadd_round_pch) (HALF_MASK, HF(res),
					   HF(src1), HF(src2), _ROUND_NINT);
  CHECK_RESULT (&res, &exp, N_ELEMS, _maskz_fcmadd_pch);
#endif

  if (n_errs != 0) {
      abort ();
  }
}

