/*
 * CODE_BASE is where we are linked; we expect to be loaded there.
 */
#define CODE_BASE	0x7c00

/*
 * POOL_BASE is a pool of memory which we can freely use; the goal is
 * to make the loader image smaller by just grabbing memory we need
 * from the pool.
 *
 * Note that we only use this base pool for the stack and E820 map; once we
 * have obtained it, we use the final few MB for temporary loader storage as
 * we can be certain it will not be used.
 */
#define POOL_BASE	0x10000

/* Realmode stack address*/
#define REALMODE_STACK	0x2000

/* Protected mode stack base */
#define PMODE_STACK	0x8000
