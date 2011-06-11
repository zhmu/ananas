#include <ananas/types.h>

#ifndef __VBE_H__
#define __VBE_H__

struct VbeInfoBlock {
	uint32_t	VbeSignature;
#define VBEINFO_SIGNATURE_VESA			0x41534556	/* "VESA" */
#define VBEINFO_SIGNATURE_VBE2			0x32454256	/* "VBE2" */
	uint16_t	VbeVersion;
	uint32_t	OemStringPtr;
	uint32_t	Capabilities;
#define VBEINFO_CAP_DAC_SWITCHABLE		(1 << 0)
#define VBEINFO_CAP_NOT_VGA_COMPITABLE		(1 << 1)
#define VBEINFO_CAP_BLANK_BIT			(1 << 2)
#define VBEINFO_CAP_STEREOSCOPIC		(1 << 3)
#define VBEINFO_CAP_STEREO_SIGNALLING		(1 << 4)
	uint32_t	VideoModePtr;
	uint16_t	TotalMemory;
	uint16_t	OemSoftwareRev;
	uint32_t	OemVendorNamePtr;
	uint32_t	OemProductNamePtr;
	uint32_t	OemProductRevPtr;
	uint8_t		Reserved[222];
	uint8_t		OemData[256];
} __attribute__((packed));

struct ModeInfoBlock {
	uint16_t	ModeAttributes;
#define VBE_MODEATTR_SUPPORTED		(1 << 0)
#define VBE_MODEATTR_BIOS_TTYOUTPUT	(1 << 2)
#define VBE_MODEATTR_COLOR		(1 << 3)
#define VBE_MODEATTR_GRAPHICS		(1 << 4)
#define VBE_MODEATTR_VGA_COMPATIBLE	(1 << 5)
#define VBE_MODEATTR_VGA_COMP_WINDOWED	(1 << 6)
#define VBE_MODEATTR_FRAMEBUFFER	(1 << 7)
#define VBE_MODEATTR_DOUBLESCAN		(1 << 8)
#define VBE_MODEATTR_INTERLACED		(1 << 9)
#define VBE_MODEATTR_TRIPLE_BUFFER	(1 << 10)
#define VBE_MODEATTR_STEREOSCOPIC	(1 << 11)
#define VBE_MODEATTR_DUAL_DISPLAY	(1 << 12)
	uint8_t		WinAAttributes;
	uint8_t		WinBAttributes;
	uint16_t	WinGranularity;
	uint16_t	WinSize;
	uint16_t	WinASegment;
	uint16_t	WinBSegment;
	uint32_t	WinFuncPtr;
	uint16_t	BytesPerScanLine;
	uint16_t	XResolution;
	uint16_t	YResolution;
	uint8_t		XCharSize;
	uint8_t		YCharSize;
	uint8_t		NumberOfPlanes;
	uint8_t		BitsPerPixel;
	uint8_t		NumberOfBanks;
	uint8_t		MemoryModel;
#define VBE_MEMMODEL_TEXT		0
#define VBE_MEMMODEL_CGA		1
#define VBE_MEMMODEL_HERCULES		2
#define VBE_MEMMODEL_PLANAR		3
#define VBE_MEMMODEL_PACKED_PIXEL	4
#define VBE_MEMMODEL_NONCHAIN		5
#define VBE_MEMMODEL_DIRECTCOLOR	6
#define VBE_MEMMODEL_YUV		7
	uint8_t		BankSize;
	uint8_t		NumberOfImagePages;
	uint8_t		_Reserved0;
	/* Direct Color fields (Direct/6 and YUV/7 memory models) */
	uint8_t		RedMaskSize;
	uint8_t		RedFieldPosition;
	uint8_t		GreenMaskSize;
	uint8_t		GreenFieldPosition;
	uint8_t		BlueMaskSize;
	uint8_t		BlueFieldPosition;
	uint8_t		RsvdeMaskSize;
	uint8_t		RsvdFieldPosition;
	uint8_t		DirectColorModeInfo;
	/* Mandatory information for VBE 2.0+ */
	uint32_t	PhysBasePtr;
	uint32_t	_Reserved1;
	uint16_t	_Reserved2;
	/* Mandatory information for VBE 3.0+ */
	uint16_t	LinBytesPerScanLine;
	uint8_t		BnkNumberOfImagePages;
	uint8_t		LinNumberOfImagePages;
	uint8_t		LinRedMaskSize;
	uint8_t		LinRedFieldPosition;
	uint8_t		LinGreenMaskSize;
	uint8_t		LinGreenFieldPosition;
	uint8_t		LinBlueMaskSize;
	uint8_t		LinBlueFieldPosition;
	uint8_t		LinRsvdMaskSize;
	uint8_t		LinRsvdFieldPosition;
	uint32_t	MaxPixelClock;
} __attribute__((packed));

struct CRTCInfoBlock {
	uint16_t	HorizontalTotal;
	uint16_t	HorizontalSyncStart;
	uint16_t	HorizontalSyncEnd;
	uint16_t	VertTotal;
	uint16_t	VertSyncStart;
	uint16_t	VertSyncEnd;
	uint8_t		Flags;
#define CTCINFO_FLAG_DOUBLESCAN			(1 << 0)
#define CTCINFO_FLAG_INTERLACED			(1 << 1)
#define CTCINFO_FLAG_HSYNC_NEGATIVE		(1 << 2)
#define CTCINFO_FLAG_VSYNC_NEGATIVE		(1 << 3)
	uint32_t	PixelClock;
	uint16_t	RefreshRate;
	uint8_t		Reserved[40];
	
} __attribute__((packed));

struct VBE_MODE {
	int		mode;
#define VBEMODE_NONE	0xffff
	int		x_res;
	int		y_res;
	int		bpp;
	int		flags;
#define VBEMODE_FLAG_PALETTE			(1 << 0)
	addr_t		framebuffer;
};

#define VBE_SETMODE_CRTC			(1 << 11)
#define VBE_SETMODE_FLAT_FRAMEBUFFER		(1 << 14)
#define VBE_SETMODE_NO_CLEAR			(1 << 15)

struct BOOTINFO;

void vbe_init();
int vbe_setmode(struct BOOTINFO* bootinfo);

#endif /* __VBE_H__ */
