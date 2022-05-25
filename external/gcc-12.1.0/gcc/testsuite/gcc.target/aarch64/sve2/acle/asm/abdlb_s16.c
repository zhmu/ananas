/* { dg-final { check-function-bodies "**" "" "-DCHECK_ASM" } } */

#include "test_sve_acle.h"

/*
** abdlb_s16_tied1:
**	sabdlb	z0\.h, z0\.b, z1\.b
**	ret
*/
TEST_TYPE_CHANGE_Z (abdlb_s16_tied1, svint16_t, svint8_t,
		    z0_res = svabdlb_s16 (z0, z1),
		    z0_res = svabdlb (z0, z1))

/*
** abdlb_s16_tied2:
**	sabdlb	z0\.h, z1\.b, z0\.b
**	ret
*/
TEST_TYPE_CHANGE_Z (abdlb_s16_tied2, svint16_t, svint8_t,
		    z0_res = svabdlb_s16 (z1, z0),
		    z0_res = svabdlb (z1, z0))

/*
** abdlb_s16_untied:
**	sabdlb	z0\.h, z1\.b, z2\.b
**	ret
*/
TEST_TYPE_CHANGE_Z (abdlb_s16_untied, svint16_t, svint8_t,
		    z0_res = svabdlb_s16 (z1, z2),
		    z0_res = svabdlb (z1, z2))

/*
** abdlb_w0_s16_tied1:
**	mov	(z[0-9]+\.b), w0
**	sabdlb	z0\.h, z0\.b, \1
**	ret
*/
TEST_TYPE_CHANGE_ZX (abdlb_w0_s16_tied1, svint16_t, svint8_t, int8_t,
		     z0_res = svabdlb_n_s16 (z0, x0),
		     z0_res = svabdlb (z0, x0))

/*
** abdlb_w0_s16_untied:
**	mov	(z[0-9]+\.b), w0
**	sabdlb	z0\.h, z1\.b, \1
**	ret
*/
TEST_TYPE_CHANGE_ZX (abdlb_w0_s16_untied, svint16_t, svint8_t, int8_t,
		     z0_res = svabdlb_n_s16 (z1, x0),
		     z0_res = svabdlb (z1, x0))

/*
** abdlb_11_s16_tied1:
**	mov	(z[0-9]+\.b), #11
**	sabdlb	z0\.h, z0\.b, \1
**	ret
*/
TEST_TYPE_CHANGE_Z (abdlb_11_s16_tied1, svint16_t, svint8_t,
		    z0_res = svabdlb_n_s16 (z0, 11),
		    z0_res = svabdlb (z0, 11))

/*
** abdlb_11_s16_untied:
**	mov	(z[0-9]+\.b), #11
**	sabdlb	z0\.h, z1\.b, \1
**	ret
*/
TEST_TYPE_CHANGE_Z (abdlb_11_s16_untied, svint16_t, svint8_t,
		    z0_res = svabdlb_n_s16 (z1, 11),
		    z0_res = svabdlb (z1, 11))
