#ifndef __SIGSET_T_DEFINED
/* Broad definition to allow future extension (i.e. real-time signals) once we need them */
typedef struct {
	unsigned int	sig[1];
} sigset_t;
#define __SIGSET_T_DEFINED
#endif
