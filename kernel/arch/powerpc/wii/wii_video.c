/*
 * Wii Video driver
 *
 * This will just do a basic setup by programming the appropriate registers,
 * map a framebuffer and blanking the screen.
 *
 * Note that the Wii has a peculiar framebuffer: it is not in RGB format, but
 * rather in YUV2 - likely to preserve memory. This means that two neighboring
 * pixels take only 32 bits in memory.
 */
#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/wii/vi.h>
#include <ananas/wii/video.h>
#include <ananas/lib.h>
#include <ananas/vm.h>

static volatile void* VI = (volatile void*)0xCC002000;	/* XXX wii? */

typedef volatile uint16_t vuint16_t;
typedef volatile uint32_t vuint32_t;

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 576

struct video_registers {
	uint16_t	vtr;
	uint16_t	dcr;
	uint32_t	htr0, htr1;
	uint32_t	vto, vte;
	uint32_t	bbei, bboi;
	uint32_t	tfbl, tfbr;
	uint32_t	bfbl, bfbr;
	uint32_t	di0, di1, di2, di3;
	uint32_t	dl0, dl1;
	uint16_t	hsw, hsr;
	uint32_t	_u68;
	uint16_t	viclk, visel;
	uint16_t	_u70;
	uint16_t	hbe, hbs;
	uint16_t	_u76;
	uint32_t	_u78, _u7c;
};

/*
 * Filter Coefficient Table values; used for anti-aliasing.
 */
static uint32_t fct_values[7] = {
	/* fct0 = */ VI_FCT0_TAP2 (430) | VI_FCT0_TAP1 (476) | VI_FCT0_TAP0 (496),
	/* fct1 = */ VI_FCT1_TAP5 (219) | VI_FCT1_TAP4 (297) | VI_FCT1_TAP3 (372),
	/* fct2 = */ VI_FCT2_TAP8 ( 12) | VI_FCT2_TAP7 ( 70) | VI_FCT2_TAP6 (142),
	/* fct3 = */ VI_FCT3_TAP12(196) | VI_FCT3_TAP11(192) | VI_FCT3_TAP10(203) | VI_FCT3_TAP9 (226),
	/* fct4 = */ VI_FCT4_TAP16(252) | VI_FCT4_TAP15(236) | VI_FCT4_TAP14(222) | VI_FCT4_TAP13(207),
	/* fct5 = */ VI_FCT5_TAP20( 19) | VI_FCT5_TAP19( 19) | VI_FCT5_TAP18( 15) | VI_FCT5_TAP17(  8),
	/* fct6 = */ VI_FCT6_TAP24(  0) | VI_FCT6_TAP23(  8) | VI_FCT6_TAP22( 12) | VI_FCT6_TAP21( 15)
};

static struct video_registers vr_ntsc = {
	.vtr   = VI_VTR_ACV(240) | VI_VTR_EQU(5),
	.dcr   = VI_DCT_ENB,
	.htr0  = VI_HTR0_HCS(71) | VI_HTR0_HCE(105) | VI_HTR0_HLW(429),
	.htr1  = VI_HTR1_HBS(373) | VI_HTR1_HBE(162) | VI_HTR1_HSY(64),
	.vto   = VI_VTO_PSR(3) | VI_VTO_PRB(24),
	.vte   = VI_VTE_PSR(2) | VI_VTE_PRB(25),
	.bbei  = VI_BBEI_BE3(520) | VI_BBEI_BS3(12) | VI_BBEI_BE1(520) | VI_BBEI_BS1(12),
	.bboi  = VI_BBOI_BE4(519) | VI_BBOI_BS4(13) | VI_BBOI_BE2(519) | VI_BBOI_BS2(13),
	.di0   = VI_DI0_ENB | VI_DI0_VCT(263) | VI_DI0_HCT(430),
	.di1   = VI_DI0_VCT(1) | VI_DI0_HCT(1),
	.di2   = VI_DI0_VCT(1) | VI_DI0_HCT(1),
	.di3   = VI_DI0_VCT(1) | VI_DI0_HCT(1),
	.dl0   = 0,
	.dl1   = 0,
	.hsw   = VI_HSW_SRCWIDTH(80) | 0x2800 /* ? */,
	.hsr   = VI_HSR_STP(256),
	._u68 = 0x00ff0000,
	.viclk = VI_VICLK_27MHZ,
	.visel = 0,
	._u70  = 640,
	.hbe   = 0,
	.hbs   = 0,
	._u76  = 0xff, ._u78 = 0x00ff00ff, ._u7c = 0x00ff00ff
};

static struct video_registers vr_pal50 = {
	.vtr   = VI_VTR_ACV(287) | VI_VTR_EQU(5),
	.dcr   = VI_DCR_FMT_PAL | VI_DCT_ENB,
	.htr0  = VI_HTR0_HCS(75) | VI_HTR0_HCE(106) | VI_HTR0_HLW(432),
	.htr1  = VI_HTR1_HBS(380) | VI_HTR1_HBE(172) | VI_HTR1_HSY(64),
	.vto   = VI_VTO_PSR(1) | VI_VTO_PRB(35),
	.vte   = VI_VTE_PSR(0) | VI_VTE_PRB(36),
	.bbei  = VI_BBEI_BE3(617) | VI_BBEI_BS3(11) | VI_BBEI_BE1(619) | VI_BBEI_BS1(13),
	.bboi  = VI_BBOI_BE4(620) | VI_BBOI_BS4(10) | VI_BBOI_BE2(618) | VI_BBOI_BS2(12),
	.di0   = VI_DI0_ENB | VI_DI0_VCT(313) | VI_DI0_HCT(433),
	.di1   = VI_DI0_ENB | VI_DI0_VCT(1) | VI_DI0_HCT(1),
	.di2   = VI_DI0_VCT(1) | VI_DI0_HCT(1),
	.di3   = VI_DI0_VCT(1) | VI_DI0_HCT(1),
	.dl0   = 0,
	.dl1   = 0,
	.hsw   = VI_HSW_SRCWIDTH(80) | 0x2800 /* ? */,
	.hsr   = VI_HSR_STP(256),
	._u68 = 0x00ff0000,
	.viclk = VI_VICLK_27MHZ,
	.visel = 0,
	._u70  = 640,
	.hbe   = 0,
	.hbs   = 0,
	._u76  = 0xff, ._u78 = 0x00ff00ff, ._u7c = 0x00ff00ff
};

static void
video_init(struct video_registers* vr, uint32_t xfb_base)
{
#define VI_WRITE16(reg, val) \
	*(vuint16_t*)(VI + (reg)) = val;
#define VI_WRITE32(reg, val) \
	*(vuint32_t*)(VI + (reg)) = val;

	VI_WRITE16(VI_VTR,   vr->vtr);
	VI_WRITE16(VI_DCR,   vr->dcr);
	VI_WRITE32(VI_HTR0,  vr->htr0);
	VI_WRITE32(VI_HTR1,  vr->htr1);
	VI_WRITE32(VI_VTO,   vr->vto);
	VI_WRITE32(VI_VTE,   vr->vte);
	VI_WRITE32(VI_BBEI,  vr->bbei);
	VI_WRITE32(VI_BBOI,  vr->bboi);
	VI_WRITE32(VI_DI0,   vr->di0);
	VI_WRITE32(VI_DI1,   vr->di1);
	VI_WRITE32(VI_DI2,   vr->di2);
	VI_WRITE32(VI_DI3,   vr->di3);
	VI_WRITE32(VI_DL0,   vr->dl0);
	VI_WRITE32(VI_DL1,   vr->dl1);
	VI_WRITE16(VI_HSW,   vr->hsw);
	VI_WRITE16(VI_HSR,   vr->hsr);
	for (unsigned int i = 0; i < sizeof(fct_values) / sizeof(uint32_t); i++)
		VI_WRITE32(VI_FCT(i), fct_values[i]);
	VI_WRITE32(VI_U68,   vr->_u68);
	VI_WRITE16(VI_VICLK, vr->viclk);
	VI_WRITE16(VI_VISEL, vr->visel);
	VI_WRITE16(VI_U70,   vr->_u70);
	VI_WRITE16(VI_HBE,   vr->hbe);
	VI_WRITE16(VI_HBS,   vr->hbs);
	VI_WRITE16(VI_U76,   vr->_u76);
	VI_WRITE32(VI_U78,   vr->_u78);
	VI_WRITE32(VI_U7C,   vr->_u7c);

	/* Set framebuffer address */
	VI_WRITE32(VI_TFBL,  VI_TFBL_PAGEOFFS | (xfb_base >> 5));
	xfb_base += 2 * SCREEN_WIDTH; /* XXX Why is this necessary? */
	VI_WRITE32(VI_BFBL,  VI_TFBL_PAGEOFFS | (xfb_base >> 5));

#undef VI_WRITE16
#undef VI_WRITE32
}

int
wiivideo_init()
{
	/* XXX PAL50 is hardcoded */
	video_init(&vr_pal50, VI_XFB_BASE);

	uint32_t* fb = wiivideo_get_framebuffer();
	for (int i = 0; i < (SCREEN_HEIGHT * SCREEN_WIDTH) / 2; i++) {
		fb[i] = 0x00800080; // black
	}
	return 0;
}

void*
wiivideo_get_framebuffer()
{
	return (void*)(0xc0000000 | VI_XFB_BASE);
}

void
wiivideo_get_size(int* height, int* width)
{
	*height = SCREEN_HEIGHT;
	*width  = SCREEN_WIDTH;
}

struct DRIVER drv_wiivideo = {
	.name       = "wiivideo",
	.drv_probe	= NULL,
	.drv_attach = NULL
};

DRIVER_PROBE(wiivideo)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
