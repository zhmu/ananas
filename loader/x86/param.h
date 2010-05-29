/*
 * CODE_BASE is where we are loaded, REALLOC_BASE is where the code was
 * linked at. We have to alter every offset throughout this file :-(
 */
#define CODE_BASE	0x7c00
#define REALLOC_BASE	0x1000

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
