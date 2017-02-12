#include <machine/debug.h>
#include <machine/pcpu.h>
#include <ananas/pcpu.h>
#include <ananas/thread.h>
#include <ananas/lib.h>

static inline void
md_set_dr(int n, uint32_t val)
{
	switch(n) {
		case 0: DR_SET(0, val); return;
		case 1: DR_SET(1, val); return;
		case 2: DR_SET(2, val); return;
		case 3: DR_SET(3, val); return;
	}
	panic("invalid dr %i", n);
}

int
md_thread_set_dr(thread_t* t, int len, int rw, addr_t addr)
{
	for (int n = 0; n < DR_AMOUNT; n++) {
		/* Skip debug registers that are in use */
		if ((t->md_dr[7] & DR7_L(n)) != 0)
			continue;

		t->md_dr[n] = addr;
		t->md_dr[7] = (t->md_dr[7] & ~DR7_CLR(n)) | DR7_G(n) | DR7_L(n) | DR7_LEN(len, n) | DR7_RW(rw, n);

		/* If we are setting for the current thread, activate immediately */
		if (PCPU_GET(curthread) == t) {
			md_set_dr(n, addr);
			DR_SET(7, t->md_dr[7]);
		}
		return n;
	}

	return -1;
}

void
md_thread_clear_dr(thread_t* t, int n)
{
	t->md_dr[n] = 0;
	t->md_dr[7] &= ~DR7_CLR(n);

	/* If we are setting for the current thread, activate immediately */
	if (PCPU_GET(curthread) == t) {
		DR_SET(7, t->md_dr[7]);
		md_set_dr(n, 0);
	}
}

/* vim:set ts=2 sw=2: */
