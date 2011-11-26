#include <ananas/types.h>
#include <efi.h>

EFI_SYSTEM_TABLE* gST;

EFI_STATUS
EfiEntry(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE* SystemTable)
{
	EFI_STATUS Status;
	gST = SystemTable;
	Status = gST->ConOut->OutputString(gST->ConOut, (UINT16*)L"Hello World!\n");

	return Status;
}

void
platform_putch(uint8_t ch)
{
	UINT16 u[2] = { ch, L'\0' };
	gST->ConOut->OutputString(gST->ConOut, u);
}

void*
platform_get_memory(uint32_t length)
{
	/* TODO */
	return NULL;
}

uint32_t
platform_get_memory_left()
{
	/* TODO */
	return 0;
}

int
platform_getch()
{
	/* TODO */
	return 0;
}

int
platform_check_key()
{
	/* TODO */
	return 0;
}

size_t
platform_init_memory_map()
{
	/* TODO */
	return 0;
}

int
platform_init_disks()
{
	/* TODO */
	return 0;
}

int
platform_read_disk(int disk, uint32_t lba, void* buffer, int num_bytes)
{
	/* TODO */
	return 0;
}

void
platform_reboot()
{
	/* TODO */
}

void
platform_cleanup()
{
}

void
platform_exec(struct LOADER_ELF_INFO* loadinfo, struct BOOTINFO* bootinfo)
{
}

int
platform_is_numbits_capable(int bits)
{
	/* TODO */
	return 0;
}

void
platform_map_memory(void* ptr, size_t len)
{
	/* Not needed for x86; we run without paging so any memory is available */
}

void
platform_delay(int ms)
{
	/* TODO */
}

void
platform_init_video()
{
}

void
platform_init_netboot()
{
	/* TODO? */
}

/* vim:set ts=2 sw=2: */
