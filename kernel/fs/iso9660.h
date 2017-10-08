#include <ananas/types.h>

#ifndef __ISO9660_H__
#define __ISO9660_H__

struct ISO9660_PRIMARY_VOLUME_DESCR {
	uint8_t		pv_typecode;		/* volume descriptor type */
	uint8_t		pv_stdentry[5];		/* standard identifier (CD001) */
	uint8_t		pv_version;		/* descriptor version (1) */
	uint8_t		pv_unused1;		/* unused */
	uint8_t		pv_sysid[32];		/* system identifier */
	uint8_t		pv_volid[32];		/* volume identifier */
	uint8_t		pv_unused2[8];		/* unused */
	uint8_t		pv_volsize[8];		/* volume size */
	uint8_t		pv_unused3[32];		/* unused */
	uint8_t		pv_setsize[4];		/* volume set size */
	uint8_t		pv_volsequence[4];	/* volume sequence in set */
	uint8_t		pv_blocksize[4];	/* logical block size */
	uint8_t		pv_pathtabsize[8];	/* path table size */
	uint8_t		pv_lsb_l_pathtab[4];	/* LBA of path table (LSB-first) */
	uint8_t		pv_opt_l_pathtab[4];	/* LBA of optional path table (LSB-first) */
	uint8_t		pv_msb_pathtab[4];	/* LBA of path table (MSB-first) */
	uint8_t		pv_opt_m_pathtab[4];	/* LBA of optional path table (MSB-first) */
	uint8_t		pv_root_direntry[34];	/* Root directory record */
	uint8_t		pv_volsetid[128];	/* Volume set identifier */
	uint8_t		pv_publisherid[128];	/* Publisher identifier */
	uint8_t		pv_preparerid[128];	/* Data preparer identifier */
	uint8_t		pv_applicationid[128];	/* Application identifier */
	uint8_t		pv_copyright_fileid[38]; /* Copyright file identifier */
	uint8_t		pv_abstract_file_id[36]; /* Abstract file identifier */
	uint8_t		pv_biblio_file_id[37];	/* Bibliography file identifier */
	uint8_t		pv_volume_creation[17];	/* Volume creation date and time */
	uint8_t		pv_volume_modification[17]; /* Volume modification date and time */
	uint8_t		pv_volume_expiration[17]; /* Volume expiration date and time */
	uint8_t		pv_volume_effective[17]; /* Volume effective date and time */
	uint8_t		pv_structure_version;	/* File structure version (1) */
	uint8_t		pv_unused4;		/* Unused */
	uint8_t		pv_application[512];	/* Application used */
	uint8_t		pv_reserved[653];	/* Reserved */
} __attribute__((packed));

struct ISO9660_DIRECTORY_ENTRY {
	uint8_t		de_length;		/* Length in bytes */
	uint8_t		de_ea_length;		/* Extended Attribute length in bytes */
	uint8_t		de_extent_lba[8];	/* Location of extent */
	uint8_t		de_data_length[8];	/* Data length */
	uint8_t		de_time[7];		/* Recording date and time */
	uint8_t		de_flags;		/* File flags */
#define DE_FLAG_HIDDEN		0x01		 /* Hidden entry */
#define DE_FLAG_DIRECTORY	0x02		 /* Directory */
#define DE_FLAG_ASSOCFILE	0x04		 /* Associated File */
#define DE_FLAG_RECORD		0x08		 /* Record File */
#define DE_FLAG_PROTECTION	0x10		 /* Protected File */
#define DE_FLAG_MULTIEXTENT	0x80		 /* Multi-Extent */
	uint8_t		de_unitsize;		/* Unit size if recorded interleaved */
	uint8_t		de_interleave_gap;	/* Interleave gap size */
	uint8_t		de_vol_seqno[4];	/* Volume sequence number */
	uint8_t		de_filename_len;	/* Length of the filename */
	uint8_t		de_filename[1];		/* Filename */
} __attribute__((packed));

#endif
