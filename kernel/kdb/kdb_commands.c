#include <ananas/kdb.h>
#include <ananas/thread.h>
#include <ananas/bio.h>

void
kdb_cmd_threads(int num_args, char** arg)
{
	thread_dump();
}

void
kdb_cmd_bio(int num_args, char** arg)
{
	bio_dump();
}

/* vim:set ts=2 sw=2: */
