#include <ananas/vmspace.h>
#include <machine/param.h>
#include <machine/vm.h>
#include <ananas/error.h>
#include <ananas/lib.h>
#include <ananas/vm.h>
#include <ananas/trace.h>

TRACE_SETUP;

errorcode_t
md_vmspace_init(vmspace_t* vs)
{
	struct PAGE* pagedir_page;
	vs->vs_md_pagedir = static_cast<uint64_t*>(page_alloc_single_mapped(&pagedir_page, VM_FLAG_READ | VM_FLAG_WRITE));
	if (vs->vs_md_pagedir == NULL)
		return ANANAS_ERROR(OUT_OF_MEMORY);
	LIST_APPEND(&vs->vs_pages, pagedir_page);

	/* Map the kernel pages in there */
	memset(vs->vs_md_pagedir, 0, PAGE_SIZE);
	md_map_kernel(vs);

	return ananas_success();
}

void
md_vmspace_destroy(vmspace_t* vs)
{
}
