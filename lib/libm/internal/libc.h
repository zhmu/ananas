#ifndef LIBC_H
#define LIBC_H

#undef weak_alias
#define weak_alias(old, new) extern __typeof(old) new __attribute__((weak, alias(#old)))

#endif /* LIBC_H */
