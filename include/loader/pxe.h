#include <sys/types.h>

#ifndef __PXE_H__
#define __PXE_H__

struct PXE_ENVPLUS {
	uint8_t   Signature[6];        /* "PXENV+" */
	uint16_t	Version;             /* Version, must be >= 0x201 */
	uint8_t		Length;              /* Length in bytes */
	uint8_t		Checksum;            /* Checksum */
	uint32_t	RMEntry;             /* Realmode entry point */
	uint32_t	PMOffset;            /* Do not use */
	uint16_t	PMSelector;          /* Do not use */
	uint16_t	StackSeg;            /* Stack segment address */
	uint16_t	StackSize;           /* Stack segment size in bytes */
	uint16_t	BC_CodeSeg;          /* BC code segment address */
	uint16_t	BC_CodeSize;         /* BC code segment size */
	uint16_t	BC_DataSeg;          /* BC data segment address */
	uint16_t	BC_DataSize;         /* BC data segment size */
	uint16_t	UNDIDataSeg;         /* UNDI data segment address */
	uint16_t	UNDIDataSize;        /* UNDI data segment size */
	uint16_t	UNDICodeSeg;         /* UNDI code segment address */
	uint16_t	UNDICodeSize;        /* UNDI code segment size */
	uint16_t	PXEPtr;              /* Realmode segment to !PXE structure */
} __attribute__((packed));

struct PXE_BANGPXE {
	uint8_t   Signature[4];        /* '!PXE' */
	uint8_t		StructLength;        /* Length in bytes */
	uint8_t		StructCksum;         /* Checksum */
	uint8_t		StructRev;           /* Revision (0) */
	uint8_t		_Reserved1;          /* 0 */
	uint32_t	UNDIROMID;           /* UNDI ROM ID structure address */
	uint32_t	BASEROMID;           /* BaseROM ID structure address */
	uint32_t	EntryPointSP;        /* PXE API entry point (real mode) */
	uint32_t	EntryPointESP;       /* PXE API entry point (protected mode) */
	uint32_t	StatusCallout;       /* DHCP/TFTP callout procedure */
	uint8_t		_Reserved2;          /* 0 */
	uint8_t		SegDescCnt;          /* Number of descriptors needed */
	uint32_t	FirstSelector;       /* First protected mode descriptor for PXE */
} __attribute__((packed));

#define PXENV_GET_CACHED_INFO 0x0071
struct PXENV_CACHED_INFO {
	uint16_t	Status;
	uint16_t	PacketType;
#define PXENV_PACKET_TYPE_DHCP_DISCOVER 1
#define PXENV_PACKET_TYPE_DHCP_ACK      2
#define PXENV_PACKET_TYPE_CACHED_REPLY  3
	uint16_t	BufferSize;
	uint16_t	BufferOffset;
	uint16_t	BufferSegment;
	uint16_t	BufferLimit;
} __attribute__((packed));

#define PXENV_EXIT_SUCCESS 0x0
#define PXENV_EXIT_FAILURE 0x1

struct BOOTP {
	uint8_t  opcode;                /* Message opcode */
#define BOOTP_REQ 1               /* Request */
#define BOOTP_REP 2               /* Response */
	uint8_t  hardware;              /* Hardware type */
	uint8_t  hardlen;               /* Hardware address length */
	uint8_t  gatehops;              /* Used for relaying */
	uint32_t ident;                 /* Transaction ID */
	uint16_t seconds;               /* Seconds elapsed since process */
	uint16_t flags;                 /* BOOTP/DHCP broadcast flags */
#define BOOTP_BCAST 0x8000
	uint32_t cip;                   /* Client IP addresss */
	uint32_t yip;                   /* 'Your' IP addresss */
	uint32_t sip;                   /* Server IP addresss */
	uint32_t gip;                   /* Gateway IP addresss */
	uint8_t  caddr[6];              /* Client hardware address */
	uint8_t  sname[64];             /* Server host name */
	uint8_t  bootfile[128];         /* Boot file name */
};

#endif /* __PXE_H__ */
