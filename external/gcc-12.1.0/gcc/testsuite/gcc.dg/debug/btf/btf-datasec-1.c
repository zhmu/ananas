/* BTF generation of BTF_KIND_DATASEC records.

   We expect 3 DATASEC records: one for each of .data, .rodata and .bss.
   .rodata: the consts; c,e,my_cstruct
   .bss:    a,b,bigarr
   .data:   d

   The type IDs of the variables placed in each section are not deterministic
   so we cannot check them.
 */

/* { dg-do compile )  */
/* { dg-options "-O0 -gbtf -dA" } */
/* { dg-options "-O0 -gbtf -dA -msdata=none" { target { { powerpc*-*-* } && ilp32 } } } */
/* { dg-options "-O0 -gbtf -dA -G0" { target { nios2-*-* } } } */

/* Check for two DATASEC entries with vlen 3, and one with vlen 1.  */
/* { dg-final { scan-assembler-times "0xf000003\[\t \]+\[^\n\]*btt_info" 2 } } */
/* { dg-final { scan-assembler-times "0xf000001\[\t \]+\[^\n\]*btt_info" 1 } } */

/* The offset entry for each variable in a DATSEC should be 0 at compile time.  */
/* { dg-final { scan-assembler-times "0\[\t \]+\[^\n\]*bts_offset" 7 } } */

/* Check that strings for each DATASEC have been added to the BTF string table.  */
/* { dg-final { scan-assembler-times "ascii \".data.0\"\[\t \]+\[^\n\]*btf_aux_string" 1 } } */
/* { dg-final { scan-assembler-times "ascii \".rodata.0\"\[\t \]+\[^\n\]*btf_aux_string" 1 } } */
/* { dg-final { scan-assembler-times "ascii \".bss.0\"\[\t \]+\[^\n\]*btf_aux_string" 1 } } */

int a;
long long b;
const long unsigned int c;

int d = 137;

const int e = -55;

int bigarr[20][10];

struct c_struct {
  long x;
  char c;
};

const struct c_struct my_cstruct = {
  99,
  '?'
};
