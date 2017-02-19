#include <ananas/types.h>
#include <ananas/bio.h>
#include <ananas/lib.h>
#include <ananas/device.h>
#include <ananas/slice.h>
#include <mbr.h>

int
mbr_process(Ananas::Device* device, struct BIO* bio)
{
	struct MBR* mbr = (struct MBR*)BIO_DATA(bio);
	static_assert(sizeof(struct MBR) == 512, "MBR structure out of size");

	/* First of all, check the MBR signature. If this does not match, reject it */
	if (((mbr->signature[1] << 8) | mbr->signature[0]) != MBR_SIGNATURE)
		return 0;

	for (int n = 0; n < MBR_NUM_ENTRIES; n++) {
		struct MBR_ENTRY* entry = &mbr->entry[n];
		/* Skip any entry with an invalid bootable flag or an ID of zero */
		if (entry->bootable != 0 && entry->bootable != 0x80)
			continue;
		if (entry->system_id == 0)
			continue;

		uint32_t first_sector =
			entry->lba_start[0]       | entry->lba_start[1] << 8 |
			entry->lba_start[2] << 16 | entry->lba_start[3] << 24;
		uint32_t size =
			entry->lba_size[0]       | entry->lba_size[1] << 8 |
			entry->lba_size[2] << 16 | entry->lba_size[3] << 24;
		if (first_sector == 0 || size == 0)
			continue;

		/* Excellent, we have a partition. Add the slice */
		slice_create(device, first_sector, size);
	}

	return 0;
}

/* vim:set ts=2 sw=2: */
