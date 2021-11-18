project(m C ASM)

cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0076 NEW)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) # ??

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/sys/ananas")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/../libc/include")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/common")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${CMAKE_CURRENT_SOURCE_DIR}/../../include")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -I.")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdlib")

# TODO compare this with newlib.h
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMISSING_SYSCALL_NAMES")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_HAVE_LONG_DOUBLE=1")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_MB_CAPABLE")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_MB_LEN_MAX=8")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_LDBL_EQ_DBL")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_NO_POSIX_SPAWN")

set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS}")

#
#    ../libm/mathfp/e_acosh.c
#    ../libm/mathfp/e_atanh.c
#    ../libm/mathfp/e_hypot.c
#    ../libm/mathfp/e_j0.c
#    ../libm/mathfp/e_j1.c
#    ../libm/mathfp/e_remainder.c
#    ../libm/mathfp/e_scalb.c
#    ../libm/mathfp/ef_acosh.c
#    ../libm/mathfp/ef_atanh.c
#    ../libm/mathfp/ef_hypot.c
#    ../libm/mathfp/ef_j0.c
#    ../libm/mathfp/ef_j1.c
#    ../libm/mathfp/ef_remainder.c
#    ../libm/mathfp/ef_scalb.c
#    ../libm/mathfp/er_gamma.c
#    ../libm/mathfp/er_lgamma.c
#    ../libm/mathfp/erf_gamma.c
#    ../libm/mathfp/erf_lgamma.c
#    ../libm/mathfp/s_acos.c
#    ../libm/mathfp/s_asin.c
#    ../libm/mathfp/s_asine.c
#    ../libm/mathfp/s_asinh.c
#    ../libm/mathfp/s_atan.c
#    ../libm/mathfp/s_atan2.c
#    ../libm/mathfp/s_atangent.c
#    ../libm/mathfp/s_ceil.c
#    ../libm/mathfp/s_cos.c
#    ../libm/mathfp/s_cosh.c
#    ../libm/mathfp/s_erf.c
#    ../libm/mathfp/s_exp.c
#    ../libm/mathfp/s_exp2.c
#    ../libm/mathfp/s_fabs.c
#    ../libm/mathfp/s_floor.c
#    ../libm/mathfp/s_fmod.c
#    ../libm/mathfp/s_frexp.c
#    ../libm/mathfp/s_ispos.c
#    ../libm/mathfp/s_ldexp.c
#    ../libm/mathfp/s_log.c
#    ../libm/mathfp/s_log10.c
#    ../libm/mathfp/s_logarithm.c
#    ../libm/mathfp/s_mathcnst.c
#    ../libm/mathfp/s_numtest.c
#    ../libm/mathfp/s_pow.c
#    ../libm/mathfp/s_signif.c
#    ../libm/mathfp/s_sin.c
#    ../libm/mathfp/s_sincos.c
#    ../libm/mathfp/s_sine.c
#    ../libm/mathfp/s_sineh.c
#    ../libm/mathfp/s_sinf.c
#    ../libm/mathfp/s_sinh.c
#    ../libm/mathfp/s_sqrt.c
#    ../libm/mathfp/s_tan.c
#    ../libm/mathfp/s_tanh.c
#    ../libm/mathfp/s_tgamma.c
#    ../libm/mathfp/sf_acos.c
#    ../libm/mathfp/sf_asin.c
#    ../libm/mathfp/sf_asine.c
#    ../libm/mathfp/sf_asinh.c
#    ../libm/mathfp/sf_atan.c
#    ../libm/mathfp/sf_atan2.c
#    ../libm/mathfp/sf_atangent.c
#    ../libm/mathfp/sf_ceil.c
#    ../libm/mathfp/sf_cos.c
#    ../libm/mathfp/sf_cosh.c
#    ../libm/mathfp/sf_erf.c
#    ../libm/mathfp/sf_exp.c
#    ../libm/mathfp/sf_exp2.c
#    ../libm/mathfp/sf_fabs.c
#    ../libm/mathfp/sf_floor.c
#    ../libm/mathfp/sf_fmod.c
#    ../libm/mathfp/sf_frexp.c
#    ../libm/mathfp/sf_ispos.c
#    ../libm/mathfp/sf_ldexp.c
#    ../libm/mathfp/sf_log.c
#    ../libm/mathfp/sf_log10.c
#    ../libm/mathfp/sf_logarithm.c
#    ../libm/mathfp/sf_numtest.c
#    ../libm/mathfp/sf_pow.c
#    ../libm/mathfp/sf_signif.c
#    ../libm/mathfp/sf_sin.c
#    ../libm/mathfp/sf_sincos.c
#    ../libm/mathfp/sf_sine.c
#    ../libm/mathfp/sf_sineh.c
#    ../libm/mathfp/sf_sinh.c
#    ../libm/mathfp/sf_sqrt.c
#    ../libm/mathfp/sf_tan.c
#    ../libm/mathfp/sf_tanh.c
#    ../libm/mathfp/sf_tgamma.c
#    ../libm/mathfp/w_cabs.c
#    ../libm/mathfp/w_drem.c
#    ../libm/mathfp/w_jn.c
#    ../libm/mathfp/wf_cabs.c
#    ../libm/mathfp/wf_drem.c
#    ../libm/mathfp/wf_jn.c
add_library(${PROJECT_NAME} SHARED
    common/acoshl.c
    common/acosl.c
    common/asinhl.c
    common/asinl.c
    common/atan2l.c
    common/atanhl.c
    common/atanl.c
    common/cbrtl.c
    common/ceill.c
    common/copysignl.c
    common/cosf.c
    common/coshl.c
    common/cosl.c
    common/erfcl.c
    common/erfl.c
    common/exp.c
    common/exp2.c
    common/exp2l.c
    common/exp_data.c
    common/expl.c
    common/expm1l.c
    common/fabsl.c
    common/fdiml.c
    common/floorl.c
    common/fmal.c
    common/fmaxl.c
    common/fminl.c
    common/fmodl.c
    common/frexpl.c
    common/hypotl.c
    common/ilogbl.c
    common/isgreater.c
    common/ldexpl.c
    common/lgammal.c
    common/llrintl.c
    common/llroundl.c
    common/log.c
    common/log10l.c
    common/log1pl.c
    common/log2.c
    common/log2_data.c
    common/log2l.c
    common/log_data.c
    common/logbl.c
    common/logl.c
    common/lrintl.c
    common/lroundl.c
    common/math_err.c
    common/math_errf.c
    common/modfl.c
    common/nanl.c
    common/nearbyintl.c
    common/nextafterl.c
    common/nexttoward.c
    common/nexttowardf.c
    common/nexttowardl.c
    common/pow.c
    common/pow_log_data.c
    common/powl.c
    common/remainderl.c
    common/remquol.c
    common/rintl.c
    common/roundl.c
    common/s_cbrt.c
    common/s_copysign.c
    common/s_exp10.c
    common/s_expm1.c
    common/s_fdim.c
    common/s_finite.c
    common/s_fma.c
    common/s_fmax.c
    common/s_fmin.c
    common/s_fpclassify.c
    common/s_ilogb.c
    common/s_infinity.c
    common/s_isinf.c
    common/s_isinfd.c
    common/s_isnan.c
    common/s_isnand.c
    common/s_llrint.c
    common/s_llround.c
    common/s_log1p.c
    common/s_log2.c
    common/s_logb.c
    common/s_lrint.c
    common/s_lround.c
    common/s_modf.c
    common/s_nan.c
    common/s_nearbyint.c
    common/s_nextafter.c
    common/s_pow10.c
    common/s_remquo.c
    common/s_rint.c
    common/s_round.c
    common/s_scalbln.c
    common/s_scalbn.c
    common/s_signbit.c
    common/s_trunc.c
    common/scalblnl.c
    common/scalbnl.c
    common/sf_cbrt.c
    common/sf_copysign.c
    common/sf_exp.c
    common/sf_exp10.c
    common/sf_exp2.c
    common/sf_exp2_data.c
    common/sf_expm1.c
    common/sf_fdim.c
    common/sf_finite.c
    common/sf_fma.c
    common/sf_fmax.c
    common/sf_fmin.c
    common/sf_fpclassify.c
    common/sf_ilogb.c
    common/sf_infinity.c
    common/sf_isinf.c
    common/sf_isinff.c
    common/sf_isnan.c
    common/sf_isnanf.c
    common/sf_llrint.c
    common/sf_llround.c
    common/sf_log.c
    common/sf_log1p.c
    common/sf_log2.c
    common/sf_log2_data.c
    common/sf_log_data.c
    common/sf_logb.c
    common/sf_lrint.c
    common/sf_lround.c
    common/sf_modf.c
    common/sf_nan.c
    common/sf_nearbyint.c
    common/sf_nextafter.c
    common/sf_pow.c
    common/sf_pow10.c
    common/sf_pow_log2_data.c
    common/sf_remquo.c
    common/sf_rint.c
    common/sf_round.c
    common/sf_scalbln.c
    common/sf_scalbn.c
    common/sf_trunc.c
    common/sincosf.c
    common/sincosf_data.c
    common/sinf.c
    common/sinhl.c
    common/sinl.c
    common/sl_finite.c
    common/sqrtl.c
    common/tanhl.c
    common/tanl.c
    common/tgammal.c
    common/truncl.c
    complex/cabs.c
    complex/cabsf.c
    complex/cabsl.c
    complex/cacos.c
    complex/cacosf.c
    complex/cacosh.c
    complex/cacoshf.c
    complex/cacoshl.c
    complex/cacosl.c
    complex/carg.c
    complex/cargf.c
    complex/cargl.c
    complex/casin.c
    complex/casinf.c
    complex/casinh.c
    complex/casinhf.c
    complex/casinhl.c
    complex/casinl.c
    complex/catan.c
    complex/catanf.c
    complex/catanh.c
    complex/catanhf.c
    complex/catanhl.c
    complex/catanl.c
    complex/ccos.c
    complex/ccosf.c
    complex/ccosh.c
    complex/ccoshf.c
    complex/ccoshl.c
    complex/ccosl.c
    complex/cephes_subr.c
    complex/cephes_subrf.c
    complex/cephes_subrl.c
    complex/cexp.c
    complex/cexpf.c
    complex/cexpl.c
    complex/cimag.c
    complex/cimagf.c
    complex/cimagl.c
    complex/clog.c
    complex/clog10.c
    complex/clog10f.c
    complex/clogf.c
    complex/clogl.c
    complex/conj.c
    complex/conjf.c
    complex/conjl.c
    complex/cpow.c
    complex/cpowf.c
    complex/cpowl.c
    complex/cproj.c
    complex/cprojf.c
    complex/cprojl.c
    complex/creal.c
    complex/crealf.c
    complex/creall.c
    complex/csin.c
    complex/csinf.c
    complex/csinh.c
    complex/csinhf.c
    complex/csinhl.c
    complex/csinl.c
    complex/csqrt.c
    complex/csqrtf.c
    complex/csqrtl.c
    complex/ctan.c
    complex/ctanf.c
    complex/ctanh.c
    complex/ctanhf.c
    complex/ctanhl.c
    complex/ctanl.c
    fenv/fe_dfl_env.c
    fenv/feclearexcept.c
    fenv/fegetenv.c
    fenv/fegetexceptflag.c
    fenv/fegetround.c
    fenv/feholdexcept.c
    fenv/fenv_stub.c
    fenv/feraiseexcept.c
    fenv/fesetenv.c
    fenv/fesetexceptflag.c
    fenv/fesetround.c
    fenv/fetestexcept.c
    fenv/feupdateenv.c
    math/e_acos.c
    math/e_acosh.c
    math/e_asin.c
    math/e_atan2.c
    math/e_atanh.c
    math/e_cosh.c
    math/e_exp.c
    math/e_fmod.c
    math/e_hypot.c
    math/e_j0.c
    math/e_j1.c
    math/e_jn.c
    math/e_log.c
    math/e_log10.c
    math/e_pow.c
    math/e_rem_pio2.c
    math/e_remainder.c
    math/e_scalb.c
    math/e_sinh.c
    math/e_sqrt.c
    math/e_tgamma.c
    math/ef_acos.c
    math/ef_acosh.c
    math/ef_asin.c
    math/ef_atan2.c
    math/ef_atanh.c
    math/ef_cosh.c
    math/ef_exp.c
    math/ef_fmod.c
    math/ef_hypot.c
    math/ef_j0.c
    math/ef_j1.c
    math/ef_jn.c
    math/ef_log.c
    math/ef_log10.c
    math/ef_pow.c
    math/ef_rem_pio2.c
    math/ef_remainder.c
    math/ef_scalb.c
    math/ef_sinh.c
    math/ef_sqrt.c
    math/ef_tgamma.c
    math/el_hypot.c
    math/er_lgamma.c
    math/erf_lgamma.c
    math/k_cos.c
    math/k_rem_pio2.c
    math/k_sin.c
    math/k_standard.c
    math/k_tan.c
    math/kf_cos.c
    math/kf_rem_pio2.c
    math/kf_sin.c
    math/kf_tan.c
    math/s_asinh.c
    math/s_atan.c
    math/s_ceil.c
    math/s_cos.c
    math/s_erf.c
    math/s_fabs.c
    math/s_floor.c
    math/s_frexp.c
    math/s_ldexp.c
    math/s_signif.c
    math/s_sin.c
    math/s_tan.c
    math/s_tanh.c
    math/sf_asinh.c
    math/sf_atan.c
    math/sf_ceil.c
    math/sf_cos.c
    math/sf_erf.c
    math/sf_fabs.c
    math/sf_floor.c
    math/sf_frexp.c
    math/sf_ldexp.c
    math/sf_signif.c
    math/sf_sin.c
    math/sf_tan.c
    math/sf_tanh.c
    math/w_acos.c
    math/w_acosh.c
    math/w_asin.c
    math/w_atan2.c
    math/w_atanh.c
    math/w_cosh.c
    math/w_drem.c
    math/w_exp.c
    math/w_exp2.c
    math/w_fmod.c
    math/w_gamma.c
    math/w_hypot.c
    math/w_j0.c
    math/w_j1.c
    math/w_jn.c
    math/w_lgamma.c
    math/w_log.c
    math/w_log10.c
    math/w_pow.c
    math/w_remainder.c
    math/w_scalb.c
    math/w_sincos.c
    math/w_sinh.c
    math/w_sqrt.c
    math/w_tgamma.c
    math/wf_acos.c
    math/wf_acosh.c
    math/wf_asin.c
    math/wf_atan2.c
    math/wf_atanh.c
    math/wf_cosh.c
    math/wf_drem.c
    math/wf_exp.c
    math/wf_exp2.c
    math/wf_fmod.c
    math/wf_gamma.c
    math/wf_hypot.c
    math/wf_j0.c
    math/wf_j1.c
    math/wf_jn.c
    math/wf_lgamma.c
    math/wf_log.c
    math/wf_log10.c
    math/wf_log2.c
    math/wf_pow.c
    math/wf_remainder.c
    math/wf_scalb.c
    math/wf_sincos.c
    math/wf_sinh.c
    math/wf_sqrt.c
    math/wf_tgamma.c
    math/wr_gamma.c
    math/wr_lgamma.c
    math/wrf_gamma.c
    math/wrf_lgamma.c
)

# TODO machine dependent stuff for libm
#set(LIBM_SOURCES
#    ../libm/machine/i386/f_exp.c
#    ../libm/machine/i386/f_expf.c
#    ../libm/machine/i386/f_llrint.c
#    ../libm/machine/i386/f_llrintf.c
#    ../libm/machine/i386/f_llrintl.c
#    ../libm/machine/i386/f_lrint.c
#    ../libm/machine/i386/f_lrintf.c
#    ../libm/machine/i386/f_lrintl.c
#    ../libm/machine/i386/f_pow.c
#    ../libm/machine/i386/f_powf.c
#    ../libm/machine/i386/f_rint.c
#    ../libm/machine/i386/f_rintf.c
#    ../libm/machine/i386/f_rintl.c
#    ../libm/machine/i386/feclearexcept.c
#    ../libm/machine/i386/fegetenv.c
#    ../libm/machine/i386/fegetexceptflag.c
#    ../libm/machine/i386/fegetround.c
#    ../libm/machine/i386/feholdexcept.c
#    ../libm/machine/i386/fenv.c
#    ../libm/machine/i386/feraiseexcept.c
#    ../libm/machine/i386/fesetenv.c
#    ../libm/machine/i386/fesetexceptflag.c
#    ../libm/machine/i386/fesetround.c
#    ../libm/machine/i386/fetestexcept.c
#    ../libm/machine/i386/feupdateenv.c
#    ../libm/machine/x86_64/feclearexcept.c
#    ../libm/machine/x86_64/fegetenv.c
#    ../libm/machine/x86_64/fegetexceptflag.c
#    ../libm/machine/x86_64/fegetround.c
#    ../libm/machine/x86_64/feholdexcept.c
#    ../libm/machine/x86_64/fenv.c
#    ../libm/machine/x86_64/feraiseexcept.c
#    ../libm/machine/x86_64/fesetenv.c
#    ../libm/machine/x86_64/fesetexceptflag.c
#    ../libm/machine/x86_64/fesetround.c
#    ../libm/machine/x86_64/fetestexcept.c
#    ../libm/machine/x86_64/feupdateenv.c
#)
target_sources(${PROJECT_NAME} PUBLIC ${LIBM_SOURCES})

install(TARGETS ${PROJECT_NAME} DESTINATION usr/lib)
target_link_libraries(${PROJECT_NAME} PRIVATE "-lc")
target_link_libraries(${PROJECT_NAME} PRIVATE "-lgcc")
