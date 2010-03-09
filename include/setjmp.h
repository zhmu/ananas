#ifndef __SETJMP_H__
#define __SETJMP_H__

/* jmp_buf is architecture-dependant */
#include <machine/setjmp.h>

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#endif /* __SETJMP_H__ */
