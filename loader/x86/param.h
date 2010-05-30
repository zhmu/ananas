/*
 * CODE_BASE is where we are linked; we expect to be loaded there.
 */
#define CODE_BASE	0x7c00

/*
 * Amount of stack available.
 */
#define STACK_SIZE	4096

/*
 * POOL_BASE is a pool of memory which we can freely use; the goal is
 * to make the loader image smaller by just grabbing memory we need
 * from the pool.
 */
#define POOL_BASE	0x4000
