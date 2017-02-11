#include <ananas/types.h>
#include <ananas/bus/pci.h>
#include <ananas/dev/ahci.h>
#include <ananas/dev/ahci-pci.h>
#include <ananas/dev/ata.h>
#include <ananas/dev/sata.h>
#include <ananas/device.h>
#include <ananas/dma.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/lib.h>
#include <ananas/page.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/time.h>
#include <ananas/trace.h>
#include <machine/param.h>
#include <machine/dma.h>
#include <machine/vm.h>

TRACE_SETUP;

#define AHCI_WRITE_4(reg, val) \
	*(volatile uint32_t*)(privdata->ap_addr + (reg)) = (val)
#define AHCI_READ_4(reg) \
	(*(volatile uint32_t*)(privdata->ap_addr + (reg)))

extern struct DRIVER drv_ahcipci_port;

extern void ahcipci_port_irq(device_t dev, struct AHCI_PCI_PORT* p, uint32_t pis);

irqresult_t
ahcipci_irq(device_t dev, void* context)
{
	struct AHCI_PRIVDATA* pd = dev->privdata;
	struct AHCI_PCI_PRIVDATA* privdata = pd->ahci_dev_privdata;

	uint32_t is = AHCI_READ_4(AHCI_REG_IS);
	AHCI_DPRINTF("irq: is=%x", is);

	for (unsigned int n = 0; n < 32; n++) {
		if ((is & AHCI_IS_IPS(n)) == 0)
			continue;

		/* XXX We could directly look them up */
		struct AHCI_PCI_PORT* p = NULL;
		for (unsigned int i = 0; i < privdata->ap_num_ports; i++)
			if (privdata->ap_port[i].p_num == n) {
				p = &privdata->ap_port[i];
				break;
			}

		uint32_t pis = AHCI_READ_4(AHCI_REG_PxIS(n));
		AHCI_WRITE_4(AHCI_REG_PxIS(n), pis);
		if (p != NULL && p->p_dev != NULL) {
			ahcipci_port_irq(p->p_dev, p, pis);
		} else {
			AHCI_DPRINTF("got IRQ for unsupported port %d", n);
		}
	}
	AHCI_WRITE_4(AHCI_REG_IS, is);

	return IRQ_RESULT_PROCESSED;
}

static errorcode_t
ahcipci_probe(device_t dev)
{
	struct RESOURCE* res = device_get_resource(dev, RESTYPE_PCI_CLASSREV, 0);
	if (res == NULL)
		return ANANAS_ERROR(NO_DEVICE);
	uint32_t classrev = res->r_base;

	/* Anything AHCI will do */
	if (PCI_CLASS(classrev) == PCI_CLASS_STORAGE && PCI_SUBCLASS(classrev) == PCI_SUBCLASS_SATA &&
	    PCI_REVISION(classrev) == 1 /* AHCI */)
		return ananas_success();

	/* And some specific devices which pre-date this schema */
	res = device_get_resource(dev, RESTYPE_PCI_VENDORID, 0);
	uint32_t vendor = res->r_base;
	res = device_get_resource(dev, RESTYPE_PCI_DEVICEID, 0);
	uint32_t device = res->r_base;
	if (vendor == 0x8086 && device == 0x2922) /* Intel ICH9, like what is in QEMU */
		return ananas_success();
	if (vendor == 0x8086 && device == 0x2829) /* Intel ICH8M, like what is in VirtualBox */
		return ananas_success();
	if (vendor == 0x10de && device == 0x7f4) /* NForce 630i SATA */
		return ananas_success();
	if (vendor == 0x1039 && device == 0x1185) /* SiS AHCI Controller (0106) */
		return ananas_success();
	return ANANAS_ERROR(NO_RESOURCE);
}

static errorcode_t
ahcipci_reset_port(device_t dev, struct AHCI_PCI_PORT* p)
{
	struct AHCI_PRIVDATA* pd = dev->privdata;
	struct AHCI_PCI_PRIVDATA* privdata = pd->ahci_dev_privdata;
	int n = p->p_num;

	/* Force a hard port reset */
	AHCI_WRITE_4(AHCI_REG_PxSCTL(n), AHCI_PxSCTL_DET(AHCI_DET_RESET));
	delay(100);
	AHCI_WRITE_4(AHCI_REG_PxSCTL(n), AHCI_READ_4(AHCI_REG_PxSCTL(n)) & ~AHCI_PxSCTL_DET(AHCI_DET_RESET));
	delay(100);

	/* Wait until the port shows some life */
	int port_delay = 100;
	while(port_delay > 0) {
		int detd = AHCI_PxSSTS_DETD(AHCI_READ_4(AHCI_REG_PxSSTS(n)));
		if (detd == AHCI_DETD_DEV_NO_PHY || detd == AHCI_DETD_DEV_PHY)
			break;
		delay(1);
		port_delay--;
	}
	if (port_delay == 0) {
		device_printf(dev, "port #%d not responding; no device?", n);
		return ANANAS_ERROR(NO_DEVICE);
	}

	/*
	 * Okay, we now need to clear the error register; this should allow us to
	 * transition to active mode
	 */
	AHCI_WRITE_4(AHCI_REG_PxSERR(n), 0xffffffff);

	port_delay = 1000;
	while(port_delay > 0) {
		int detd = AHCI_PxSSTS_DETD(AHCI_READ_4(AHCI_REG_PxSSTS(n)));
		if (detd == AHCI_DETD_DEV_PHY)
			break;
		delay(1);
		port_delay--;
	}
	if (port_delay == 0) {
		device_printf(dev, "port #%d doesn't go to dev/phy", n);
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Now wait for a bit until the port's BSY flag drops */
	port_delay = 1000;
	while(port_delay > 0) {
		uint32_t tfd = AHCI_READ_4(AHCI_REG_PxTFD(n));
		if ((tfd & 0x80) == 0)
			break;
		delay(10);
		port_delay--;
	}

	if (port_delay == 0)
		device_printf(dev, "port #%d alive but never clears bsy?!", n);
	else
		device_printf(dev, "port #%d okay", n);

	/* Reset any port errors */
	AHCI_WRITE_4(AHCI_REG_PxSERR(n), 0xffffffff);

	/* Clear any pending interrupts */
	AHCI_WRITE_4(AHCI_REG_PxIS(n), 0xffffffff);

	return ananas_success();
}

static errorcode_t
ahcipci_attach(device_t dev)
{
	void* res_mem = device_alloc_resource(dev, RESTYPE_MEMORY, 4096);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_mem == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	/* Enable busmastering; all communication is done by DMA */
	pci_enable_busmaster(dev, 1);

	/* XXX I wonder if the BIOS/OS handoff (10.6) should happen here? */
	if (*(volatile uint32_t*)(res_mem + AHCI_REG_CAP2) & AHCI_CAP2_BOH) {
		kprintf("NEED BIOS stuff\n");
	}

	/*
	 * While initializing, do not use AHCI_READ_ for this because we need to
	 * allocate the privdata based on the number of ports we have.
	 */
	/* Enable AHCI aware mode, but disable interrupts */
	*(volatile uint32_t*)(res_mem + AHCI_REG_GHC) = AHCI_GHC_AE;

	/* Reset the controller */
	*(volatile uint32_t*)(res_mem + AHCI_REG_GHC) |= AHCI_GHC_HR;
	int reset_wait = 1000;
	while(reset_wait > 0) {
		if ((*(volatile uint32_t*)(res_mem + AHCI_REG_GHC) & AHCI_GHC_HR) == 0)
			break;
		reset_wait--;
		delay(1);
	}
	if (reset_wait == 0) {
		device_printf(dev, "controller never leaves reset, giving up");
		return ANANAS_ERROR(NO_DEVICE);
	}
	/* Re-enable AHCI mode; this is part of what is reset */
	*(volatile uint32_t*)(res_mem + AHCI_REG_GHC) |= AHCI_GHC_AE;

	/* Count the number of ports we actually have */
	uint32_t pi = *(volatile uint32_t*)(res_mem + AHCI_REG_PI);
	int num_ports = 0;
	for (unsigned int n = 0; n < 32; n++)
		if ((pi & AHCI_PI_PI(n)) != 0)
			num_ports++;
	AHCI_DPRINTF("pi=%x -> #ports=%d", pi, num_ports);

	/* We can grab memory now */
	struct AHCI_PCI_PRIVDATA* privdata = kmalloc(sizeof(struct AHCI_PCI_PRIVDATA) + sizeof(struct AHCI_PCI_PORT) * num_ports);
	memset(privdata, 0, sizeof(*privdata) + sizeof(struct AHCI_PCI_PORT) * num_ports);
	privdata->ap_addr = (addr_t)res_mem;
	privdata->ap_pi = pi;
	privdata->ap_num_ports = num_ports;

	/* Create DMA tags; we need those to do DMA */
	errorcode_t err = dma_tag_create(dev->parent->dma_tag, dev, &dev->dma_tag, 1, 0, DMA_ADDR_MAX_32BIT, DMA_SEGS_MAX_ANY, DMA_SEGS_MAX_SIZE);
	ANANAS_ERROR_RETURN(err);

	uint32_t cap = AHCI_READ_4(AHCI_REG_CAP);
	privdata->ap_ncs = AHCI_CAP_NCS(cap) + 1;

	/* XXX */
	struct AHCI_PRIVDATA* pd = kmalloc(sizeof(*pd));
	memset(pd, 0, sizeof(*pd));
	pd->ahci_dev_privdata = privdata;
	dev->privdata = pd;

	err = irq_register((int)(uintptr_t)res_irq, dev, ahcipci_irq, IRQ_TYPE_DEFAULT, privdata);
	ANANAS_ERROR_RETURN(err);

	/* Force all ports into idle mode */
	int need_wait = 0;
	for (unsigned int n = 0; n < 32; n++) {
		if ((privdata->ap_pi & AHCI_PI_PI(n)) == 0)
			continue;

		/* Check if the port is idle; if this is true, we needn't do anything here */
		if ((AHCI_READ_4(AHCI_REG_PxCMD(n)) & (AHCI_PxCMD_ST | AHCI_PxCMD_CR | AHCI_PxCMD_FRE | AHCI_PxCMD_FR)) == 0)
			continue;

		/* Port still running; reset ST and FRE (this is acknowledged by the CR and FR being cleared) */
		AHCI_WRITE_4(AHCI_REG_PxCMD(n), (AHCI_READ_4(AHCI_REG_PxCMD(n)) & ~(AHCI_PxCMD_ST | AHCI_PxCMD_FRE)));
		need_wait++;
	}

	/* If we had to reset ports, wait until they are all okay */
	if (need_wait) {
		delay(500);
		int ok = 1;

		/* Check if they are all okay */
		for (unsigned int n = 0; n < 32; n++) {
			if ((privdata->ap_pi & AHCI_PI_PI(n)) == 0)
				continue;

			if ((AHCI_READ_4(AHCI_REG_PxCMD(n)) & (AHCI_PxCMD_CR | AHCI_PxCMD_FR)) != 0)
				ok = 0;
		}
		if (!ok) {
			/* XXX we can and should recover from this */
			device_printf(dev, "not all ports reset correctly");
			return ANANAS_ERROR(NO_DEVICE);
		}
	}

	/* Allocate memory and program buffers for all usable ports */
	int idx = 0;
	for (unsigned int n = 0; n < 32; n++) {
		if ((privdata->ap_pi & AHCI_PI_PI(n)) == 0)
			continue;
		struct AHCI_PCI_PORT* p = &privdata->ap_port[idx];
		idx++;

		/* Create DMA-able memory buffers for the command list and RFIS */
		err = dma_buf_alloc(dev->dma_tag, 1024, &p->p_dmabuf_cl);
		ANANAS_ERROR_RETURN(err);
		err = dma_buf_alloc(dev->dma_tag, 256, &p->p_dmabuf_rfis);
		ANANAS_ERROR_RETURN(err);

		spinlock_init(&p->p_lock);
		p->p_pd = privdata;
		p->p_cle = dma_buf_get_segment(p->p_dmabuf_cl, 0)->s_virt;
		p->p_rfis = dma_buf_get_segment(p->p_dmabuf_rfis, 0)->s_virt;
		p->p_num = n;

		/* Program our command list and FIS buffer addresses */
		uint64_t addr_cl = dma_buf_get_segment(p->p_dmabuf_cl, 0)->s_phys;
		AHCI_WRITE_4(AHCI_REG_PxCLB(n), addr_cl & 0xffffffff);
		AHCI_WRITE_4(AHCI_REG_PxCLBU(n), addr_cl >> 32);
		uint64_t addr_rfis = dma_buf_get_segment(p->p_dmabuf_rfis, 0)->s_phys;
		AHCI_WRITE_4(AHCI_REG_PxFB(n), addr_rfis & 0xffffffff);
		AHCI_WRITE_4(AHCI_REG_PxFBU(n), addr_rfis >> 32);
		AHCI_DPRINTF("## port %d command list at %p rfis at %p",  n, addr_cl, addr_rfis);

		/* Activate the channel */
		AHCI_WRITE_4(AHCI_REG_PxCMD(n),
		 AHCI_PxCMD_ICC(AHCI_ICC_ACTIVE) |
		 AHCI_PxCMD_POD |	AHCI_PxCMD_SUD
		);

		/*
		 * Enable receiving of FISes; we'll remain BSY without this as we need to
		 * retrieve the device's signature...
		 */
		AHCI_WRITE_4(AHCI_REG_PxCMD(n), AHCI_READ_4(AHCI_REG_PxCMD(n)) | AHCI_PxCMD_FRE);

		/* Reset the port to ensure the device is in a sane state */
		ahcipci_reset_port(dev, p);

		/* XXX For now, be extra interrupt-prone */
		AHCI_WRITE_4(AHCI_REG_PxIE(n),
		 AHCI_PxIE_CPDE | AHCI_PxIE_TFEE | AHCI_PxIE_HBFE | AHCI_PxIE_HBDE |
		 AHCI_PxIE_IFE | AHCI_PxIE_INFE | AHCI_PxIE_OFE | AHCI_PxIE_IPME |
		 AHCI_PxIE_PRCE | AHCI_PxIE_DMPE | AHCI_PxIE_PCE | AHCI_PxIE_DPE |
		 AHCI_PxIE_UFE | AHCI_PxIE_SDBE | AHCI_PxIE_DSE | AHCI_PxIE_PSE |
		 AHCI_PxIE_DHRE
		);
	}

	/* Enable global AHCI interrupts */
	AHCI_WRITE_4(AHCI_REG_IS, 0xffffffff);
	AHCI_WRITE_4(AHCI_REG_GHC, AHCI_READ_4(AHCI_REG_GHC) | AHCI_GHC_IE);

	/*
	 * Interrupts are enabled; ports should be happy - now, attach all devices on
	 * the ports.
	 */
	idx = 0;
	for (unsigned int n = 0; n < 32; n++) {
		if ((privdata->ap_pi & AHCI_PI_PI(n)) == 0)
			continue;
		struct AHCI_PCI_PORT* p = &privdata->ap_port[idx];
		idx++;

		/*
		 * Create a port device here; the port will handle device attaching,
		 * command queuing and other such things. Things like disks and ATAPI
		 * devices will connect to the port itself.
		 */
		device_t port_dev = device_alloc(dev, &drv_ahcipci_port);
		port_dev->privdata = p;
		p->p_dev = port_dev;
		device_add_resource(port_dev, RESTYPE_CHILDNUM, n, 0);
		device_attach_single(port_dev); /* XXX check error */
	}

	return ananas_success();
}

struct DRIVER drv_ahcipci = {
	.name			= "ahci-pci",
	.drv_probe		= ahcipci_probe,
	.drv_attach		= ahcipci_attach,
};

DRIVER_PROBE(ahcipci)
DRIVER_PROBE_BUS(pcibus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
