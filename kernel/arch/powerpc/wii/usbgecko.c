#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/wii/exi.h>
#include <ananas/wii/usbgecko.h>

/* We're poking registers; so ensure any access is volatile! */
typedef volatile uint32_t vuint32_t;

static int usb_gecko_channel = -1;

static uint32_t
usbgecko_transaction(int ch, uint32_t data)
{
	volatile void* exi_base = (volatile void*)EXI_ADDR(ch);

	/* 5.9.1.2: Selecting a specific EXI device: select the USB Gecko device */
	*(vuint32_t*)(exi_base + EXI_CSR) = EXI_CSR_CLK_32MHZ | EXI_CSR_CS(1);

	/* 5.9.1.4: Perform IMM operation: setup data */
	*(vuint32_t*)(exi_base + EXI_DATA) = data;

	/* 5.9.1.4: Perform IMM operation: schedule read/write; 2 bytes; start
	 * now */
	*(vuint32_t*)(exi_base + EXI_CR) = EXI_CR_TLEN_2 | EXI_CR_RW_RW | EXI_CR_TSTART;

	/* 5.9.1.4: Perform IMM operation: Wait until transfer is completed */
	while (1) {
		if ((*(vuint32_t*)(exi_base + EXI_CR) & EXI_CR_TSTART) == 0)
			break;
		/* XXX barrier */
	}

	/* 5.9.1.4: Fetch the data */
	data = *(vuint32_t*)(exi_base + EXI_DATA);

	/* 5.9.1.3: Deselecting EXI devices */
	*(vuint32_t*)(exi_base + EXI_CSR) = 0;

	return data;
}

void
usbgecko_putch(char ch)
{
	if (usb_gecko_channel < 0)
		return;

	int timeout = 16;
	while (1) {
		if (usbgecko_transaction(usb_gecko_channel, USBGECKO_CMD_TX_STATUS) & USBGECKO_TX_FIFO_READY)
			break;
		timeout--;
		if (timeout < 0) {
			break;
		}
	}
	usbgecko_transaction(usb_gecko_channel, USBGECKO_CMD_TRANSMIT(ch));
	/* XXX hack to get newlines right */
	if (ch == '\n')
		usbgecko_putch('\r');
}

void
usbgecko_init()
{
	for (int i = 0; i < 2; i++) {
		uint32_t d = usbgecko_transaction(i, USBGECKO_CMD_IDENTIFY);
		if (d != USBGECKO_IDENTIFY_RESPONSE)
			continue;
		usb_gecko_channel = i;
	}
	usbgecko_putch('H'); usbgecko_putch('i'); usbgecko_putch('!');
}

struct DRIVER drv_usbgecko = {
	.name					= "usbgecko",
	.drv_probe		= NULL,
	.drv_attach		= NULL,
	.drv_write		= NULL,
	.drv_read     = NULL
};

DRIVER_PROBE(usbgecko)
DRIVER_PROBE_BUS(foo) /* XXX so that it can't be found */
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
