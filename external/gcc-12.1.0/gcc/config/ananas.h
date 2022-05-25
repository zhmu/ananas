/* Useful if you wish to make target-specific GCC changes. */
#undef TARGET_ANANAS
#define TARGET_ANANAS 1
 
#undef LIB_SPEC
#define LIB_SPEC "-lc"
 
/* Files that are linked before user code.
   The %s tells GCC to look for these files in the library directory. */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "crt1.o%s crti.o%s %{shared|pie:crtbeginS.o%s;:crtbegin.o%s}"

/* Files that are linked after user code. */
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "%{shared|pie:crtendS.o%s;:crtend.o%s} crtn.o%s"
 
/* Additional predefined macros. */
#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()      \
  do {                                \
    builtin_define_std ("unix");         \
    builtin_define ("__Ananas__");    \
    builtin_assert ("system=unix");      \
    builtin_assert ("system=ananas"); \
  } while(0);

/* Ensure -shared / -static are passed to the linker */
#define LINK_SPEC " \
    %{shared:-shared} %{static:-static} \
    %{!static:%{rdynamic:-export-dynamic}}"

/* Use --as-needed -lgcc_s for eh support.  */
#ifdef HAVE_LD_AS_NEEDED
#define USE_LD_AS_NEEDED 1
#endif
