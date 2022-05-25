/* { dg-final { check-function-bodies "**" "" "-DCHECK_ASM" } } */

#include "test_sve_acle.h"

/*
** mullb_lane_0_u64_tied1:
**	umullb	z0\.d, z0\.s, z1\.s\[0\]
**	ret
*/
TEST_TYPE_CHANGE_Z (mullb_lane_0_u64_tied1, svuint64_t, svuint32_t,
		    z0_res = svmullb_lane_u64 (z0, z1, 0),
		    z0_res = svmullb_lane (z0, z1, 0))

/*
** mullb_lane_0_u64_tied2:
**	umullb	z0\.d, z1\.s, z0\.s\[0\]
**	ret
*/
TEST_TYPE_CHANGE_Z (mullb_lane_0_u64_tied2, svuint64_t, svuint32_t,
		    z0_res = svmullb_lane_u64 (z1, z0, 0),
		    z0_res = svmullb_lane (z1, z0, 0))

/*
** mullb_lane_0_u64_untied:
**	umullb	z0\.d, z1\.s, z2\.s\[0\]
**	ret
*/
TEST_TYPE_CHANGE_Z (mullb_lane_0_u64_untied, svuint64_t, svuint32_t,
		    z0_res = svmullb_lane_u64 (z1, z2, 0),
		    z0_res = svmullb_lane (z1, z2, 0))

/*
** mullb_lane_1_u64:
**	umullb	z0\.d, z1\.s, z2\.s\[1\]
**	ret
*/
TEST_TYPE_CHANGE_Z (mullb_lane_1_u64, svuint64_t, svuint32_t,
		    z0_res = svmullb_lane_u64 (z1, z2, 1),
		    z0_res = svmullb_lane (z1, z2, 1))

/*
** mullb_lane_2_u64:
**	umullb	z0\.d, z1\.s, z2\.s\[2\]
**	ret
*/
TEST_TYPE_CHANGE_Z (mullb_lane_2_u64, svuint64_t, svuint32_t,
		    z0_res = svmullb_lane_u64 (z1, z2, 2),
		    z0_res = svmullb_lane (z1, z2, 2))

/*
** mullb_lane_3_u64:
**	umullb	z0\.d, z1\.s, z2\.s\[3\]
**	ret
*/
TEST_TYPE_CHANGE_Z (mullb_lane_3_u64, svuint64_t, svuint32_t,
		    z0_res = svmullb_lane_u64 (z1, z2, 3),
		    z0_res = svmullb_lane (z1, z2, 3))

/*
** mullb_lane_z15_u64:
**	str	d15, \[sp, -16\]!
**	umullb	z0\.d, z1\.s, z15\.s\[1\]
**	ldr	d15, \[sp\], 16
**	ret
*/
TEST_DUAL_LANE_REG (mullb_lane_z15_u64, svuint64_t, svuint32_t, z15,
		    z0 = svmullb_lane_u64 (z1, z15, 1),
		    z0 = svmullb_lane (z1, z15, 1))

/*
** mullb_lane_z16_u64:
**	mov	(z[0-9]|z1[0-5])\.d, z16\.d
**	umullb	z0\.d, z1\.s, \1\.s\[1\]
**	ret
*/
TEST_DUAL_LANE_REG (mullb_lane_z16_u64, svuint64_t, svuint32_t, z16,
		    z0 = svmullb_lane_u64 (z1, z16, 1),
		    z0 = svmullb_lane (z1, z16, 1))
