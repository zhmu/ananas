#ifndef __ANANAS_WII_VI_H__
#define __ANANAS_WII_VI_H__

/*
 * Vertical Timing Register (16): 00aa aaaa aaaa eeee
 *
 * a = ACV - Active Video
 * e = EQU - Equalization pulse in half lines
 */
#define VI_VTR	0x0000
# define VI_VTR_ACV(x) ((x) << 4)
# define VI_VTR_EQU(x) ((x) << 0)

/*
 * Display Configuation Register (16): 0000 00pp lltt dire
 * 
 * pp = FMT - Current Video Format
 * ll = LE1 - Enable Display Latch 1
 * tt = LE0 - Enable Display Latch 0
 * d  = DLR - Select 3D Display Mode
 * i  = NIN - Interlace Selector
 * r  = RST - Reset
 * e  = ENB - Enable
 * 
 */
#define VI_DCR	0x0002
# define VI_DCR_FORMAT(x)	((x) & 0x300)
#  define VI_DCR_FMT_NTSC	(0 << 8)
#  define VI_DCR_FMT_PAL	(1 << 8)
#  define VI_DCR_FMT_MPAL	(2 << 8)
#  define VI_DCR_FMT_MDEBUG	(3 << 8)
# define VI_DCT_LE1(x)		((x) & 0xc0)
#  define VI_DCT_LE1_OFF	(0 << 6)
#  define VI_DCT_LE1_ON1	(1 << 6)
#  define VI_DCT_LE1_ON2	(2 << 6)
#  define VI_DCT_LE1_ON		(3 << 6)
# define VI_DCT_LE0(x)		((x) & 0x30)
#  define VI_DCT_LE0_OFF	(0 << 4)
#  define VI_DCT_LE0_ON1	(1 << 4)
#  define VI_DCT_LE0_ON2	(2 << 4)
#  define VI_DCT_LE0_ON		(3 << 4)
# define VI_DCT_DLR		(1 << 3)
# define VI_DCT_NIN		(1 << 2)
# define VI_DCT_RST		(1 << 1)
# define VI_DCT_ENB		(1 << 0)

/* 
 * Horizontal Timing Register 0 (32): 0sss ssss 0eee eeee 0000 000w wwww wwww
 *
 * s = HCS - Horizontal Sync Start to Color Burst Start
 * e = HCE - Horizontal Sync Start to Color Burst End
 * w = HLW - Halfline Width (W * 16 = Width)
 */
#define VI_HTR0	0x0004
# define VI_HTR0_HCS(x)		((x) << 24)
# define VI_HTR0_HCE(x)		((x) << 16)
# define VI_HTR0_HLW(x)		((x) << 0)

/* 
 * Horizontal Timing Register 1 (32): 0000 0sss ssss ssse eeee eeee ewww wwww
 *
 * s = HBS - Half line to horizontal blank start
 * e = HBE - Horizontal Sync Start to horizontal blank end
 * w = HSY - Horizontal Sync Width
 */
#define VI_HTR1	0x0008
# define VI_HTR1_HBS(x)		((x) << 17)
# define VI_HTR1_HBE(x)		((x) << 7)
# define VI_HTR1_HSY(x)		((x) << 0)

/*
 * Odd Field Vertical Timing Register (32): .... ..ss ssss ssss .... ..rr rrrr rrrr
 *
 * s = PSR - Post blanking in half lines
 * r = PRB - Pre-blanking in half lines
 */
#define VI_VTO	0x000c
# define VI_VTO_PSR(x)		((x) << 16)
# define VI_VTO_PRB(x)		((x) << 0)

/*
 * Even Field Vertical Timing Register (32): .... ..ss ssss ssss .... ..rr rrrr rrrr
 *
 * s = PSR - Post blanking in half lines
 * r = PRB - Pre-blanking in half lines
 */
#define VI_VTE	0x0010
# define VI_VTE_PSR(x)		((x) << 16)
# define VI_VTE_PRB(x)		((x) << 0)

/*
 * Odd Field Burst Blanking Interval Register (32)
 *
 * (bit 21-31) BE3 - Field 3 start to burst blanking end in halflines
 * (bit 16-20) BS3 - Field 3 start to burst blanking start in halflines
 * (bit 5-15)  BE1 - Field 1 start to burst blanking end in halflines
 * (bit 0-4)   BS1 - Field 1 start to burst blanking start in halflines
 */
#define VI_BBEI 0x0014
# define VI_BBEI_BE3(x)		((x) << 21)
# define VI_BBEI_BS3(x)		((x) << 16)
# define VI_BBEI_BE1(x)		((x) << 5)
# define VI_BBEI_BS1(x)		((x) << 0)

/*
 * Even Field Burst Blanking Interval Register (32)
 *
 * (bit 21-31) BE4 - Field 4 start to burst blanking end in halflines
 * (bit 16-20) BS4 - Field 4 start to burst blanking start in halflines
 * (bit 5-15)  BE2 - Field 2 start to burst blanking end in halflines
 * (bit 0-4)   BS2 - Field 2 start to burst blanking start in halflines
 */
#define VI_BBOI 0x0018
# define VI_BBOI_BE4(x)		((x) << 21)
# define VI_BBOI_BS4(x)		((x) << 16)
# define VI_BBOI_BE2(x)		((x) << 5)
# define VI_BBOI_BS2(x)		((x) << 0)

/*
 * Top Field Base Register (L, 32): yyyp zzzz aaaa aaaa aaax xxxx xxxx
 *
 * y = always zero
 * p = page offset bit
 * z = XOF - Horizontal Offset of the left-most pixel within the first word of the fetched picture
 * a = FBB - bits 23-9 of the XFB address
 */
#define VI_TFBL 0x001c
# define VI_TFBL_CLEAR2831	(1 << 31)
# define VI_TFBL_PAGEOFFS	(1 << 28)
# define VI_TFBL_XOF(x)		((x) << 24)
# define VI_TFBL_FBB(x)		((x) << 9)

/*
 * Top Field Base Register (R, 32): 0000 0000 ffff ffff ffff fff0 0000 0000
 *
 * f = FBB - External Memory Address of frame buffer
 */
#define VI_TFBR 0x0020

/*
 * Bottom Field Base Register (L, 32) - 00p0 0000 aaaa aaaa aaaa aaax xxxx xxxx
 *
 * p = page offset bit
 * a = FBB - bits 23-9 of XFB address
 * x = unused
 */
#define VI_BFBL 0x0024
# define VI_BFBL_CLEAR2831	(1 << 31)

/*
 * Bottom Field Base Register (R, 32) - 0000 0000 ffff ffff ffff fff0 0000 0000
 *
 * f = FBB - External Memory Address of frame buffer
 */
#define VI_BFBR 0x0028

/*
 * Current Vertical Position (2, RO) - 0000 0vvvv vvvv vvvv
 *
 * v = VCT - current vertical position of raster beam
 */
#define VI_DPV 0x002c

/*
 * Current Horizontal Position (2, RO) - 0000 0hhhh hhhh hhhh
 *
 * h = HCT - current horizontal position of raster beam
 */
#define VI_DPH 0x002e

/*
 * Display Interrupt 0 (32) - i00e 00vv vvvv vvvv 0000 00hh hhhh hhhh
 *
 * i = INT - Interrupt Status (1=Active)
 * e = ENB - Interrupt Enable Bit
 * v = VCT - Vertical Position
 * h = HCT - Horizontal Position
 */
#define VI_DI0 0x0030
# define VI_DI0_INT		(1 << 31)
# define VI_DI0_ENB		(1 << 28)
# define VI_DI0_VCT(x)		((x) << 16)
# define VI_DI0_HCT(x)		((x) << 0)

/* Display Interrupt 1-3 (32) - refer to Display Interrupt 0 */
#define VI_DI1 0x0034
#define VI_DI2 0x0038
#define VI_DI3 0x003c

/*
 * Display Latch Register 0 (32) - t000 vvvv vvvv vvvv 0000 00hh hhhh hhhh
 *
 * t = TRG - Trigger Flag
 * v = VCT - Vertical Count
 * h = HCT - Horizontal Count
 */
#define VI_DL0 0x0040

/* Display Latch Register 1 - refer to Display Latch Register 0 */
#define VI_DL1 0x0044

/*
 * Scaling Width Register (16) - 0000 00hh hhhh hhhh
 *
 * h = SRCWIDTH - Horizontal Stepping Size
 */
#define VI_HSW 0x0048
# define VI_HSW_SRCWIDTH(x) ((x) << 0)

/*
 * Horizontal Scaling Register (16) - 000e 000v vvvv vvvv
 *
 * e = HS_EN - Enable Horizontal Scaling
 * v = STP - Horizontal steppign size
 */
#define VI_HSR 0x004a
# define VI_HSR_HS_EN	(1 << 12)
# define VI_HSR_STP(x)	((x) << 0)

/*
 * Filter Coefficient Table 0 (32) - 00aa aaaa aaaa bbbb bbbb bbcc cccc cccc
 *
 * a = T2 - Tap2
 * b = T1 - Tap1
 * c = T0 - Tap0
 */
#define VI_FCT0 0x004c
# define VI_FCT0_TAP2(x) ((x) << 20)
# define VI_FCT0_TAP1(x) ((x) << 10)
# define VI_FCT0_TAP0(x) ((x) << 0)

/*
 * Filter Coefficient Table 1 (32) - 00aa aaaa aaaa bbbb bbbb bbcc cccc cccc
 *
 * a = T5 - Tap5
 * b = T4 - Tap4
 * c = T3 - Tap3
 */
#define VI_FCT1 0x0050
# define VI_FCT1_TAP5(x) ((x) << 20)
# define VI_FCT1_TAP4(x) ((x) << 10)
# define VI_FCT1_TAP3(x) ((x) << 0)

/*
 * Filter Coefficient Table 2 (32) - 00aa aaaa aaaa bbbb bbbb bbcc cccc cccc
 *
 * a = T8 - Tap8
 * b = T7 - Tap7
 * c = T6 - Tap6
 */
#define VI_FCT2 0x0054
# define VI_FCT2_TAP8(x) ((x) << 20)
# define VI_FCT2_TAP7(x) ((x) << 10)
# define VI_FCT2_TAP6(x) ((x) << 0)

/*
 * Filter Coefficient Table 3 (32) - aaaa aaaa bbbb bbbb cccc cccc dddd dddd
 *
 * a = T12 - Tap12
 * b = T11 - Tap11
 * c = T10 - Tap10
 * d = T9  - Tap9
 */
#define VI_FCT3 0x0058
# define VI_FCT3_TAP12(x) ((x) << 24)
# define VI_FCT3_TAP11(x) ((x) << 16)
# define VI_FCT3_TAP10(x) ((x) << 8)
# define VI_FCT3_TAP9(x) ( (x) << 0)

/*
 * Filter Coefficient Table 4 (32) - aaaa aaaa bbbb bbbb cccc cccc dddd dddd
 *
 * a = T16 - Tap16
 * b = T15 - Tap15
 * c = T14 - Tap14
 * d = T13 - Tap13
 */
#define VI_FCT4 0x005c
# define VI_FCT4_TAP16(x) ((x) << 24)
# define VI_FCT4_TAP15(x) ((x) << 16)
# define VI_FCT4_TAP14(x) ((x) << 8)
# define VI_FCT4_TAP13(x) ((x) << 0)

/*
 * Filter Coefficient Table 5 (32) - aaaa aaaa bbbb bbbb cccc cccc dddd dddd
 *
 * a = T20 - Tap20
 * b = T19 - Tap19
 * c = T18 - Tap18
 * d = T17 - Tap17
 */
#define VI_FCT5 0x0060
# define VI_FCT5_TAP20(x) ((x) << 24)
# define VI_FCT5_TAP19(x) ((x) << 16)
# define VI_FCT5_TAP18(x) ((x) << 8)
# define VI_FCT5_TAP17(x) ((x) << 0)

/*
 * Filter Coefficient Table 6 (32) - aaaa aaaa bbbb bbbb cccc cccc dddd dddd
 *
 * a = T24 - Tap24 (must be zero)
 * b = T23 - Tap23
 * c = T22 - Tap22
 * d = T21 - Tap21
 */
#define VI_FCT6 0x0064
# define VI_FCT6_TAP24(x) ((x) << 24)
# define VI_FCT6_TAP23(x) ((x) << 16)
# define VI_FCT6_TAP22(x) ((x) << 8)
# define VI_FCT6_TAP21(x) ((x) << 0)

/* Used to address a given fct, 0 <= x <= 6 */
#define VI_FCT(x) (0x004c + (4 * x))

/* 68 - ?? */
#define VI_U68 0x0068

/*
 * VI Clock Select Register (16) - 0000 0000 0000 0000s
 *
 * s = Video clock
 */
#define VI_VICLK 0x006c
# define VI_VICLK_27MHZ 0
# define VI_VICLK_54MHZ 1

/*
 * VI DTV Status Register
 */
#define VI_VISEL 0x006e

/* 70 - ?? */
#define VI_U70 0x0070

/*
 * Border HBE (16) - e000 00hh hhhh hhhh
 *
 * e = BRDR_EN - Border Enable
 * h = HBE656 - Border Horizontal Blank End
 */
#define VI_HBE 0x0072

/*
 * Border HBS (16) - 0000 00hh hhhh hhhh
 *
 * h = HBE656 - Border Horizontal Blank start
 */
#define VI_HBS 0x0074

/* 76 - 7c: ?? */
#define VI_U76 0x0076
#define VI_U78 0x0078
#define VI_U7C 0x007c

/* Framebuffer physical memory location */
#define VI_XFB_BASE 0x01698000

/* Framebuffer size, in bytes */
#define VI_XFB_SIZE 0x168000

#endif /* __ANANAS_WII_VI_H__ */
