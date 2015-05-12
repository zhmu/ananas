#include <ananas/types.h>
#include <ananas/dev/ahci.h>
#include <ananas/dev/ahci-pci.h>
#include <ananas/dev/sata.h>
#include <ananas/bio.h>
#include <ananas/kmem.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <machine/vm.h>

TRACE_SETUP;

#define PORT_LOCK spinlock_lock(&p->p_lock)
#define PORT_UNLOCK spinlock_unlock(&p->p_lock)

extern struct DRIVER drv_sata_disk;

void
ahcipci_port_irq(device_t dev, struct AHCI_PCI_PORT* p, uint32_t pis)
{
	struct AHCI_PCI_PRIVDATA* privdata = p->p_pd;

	AHCI_DPRINTF("got irq, pis=%x", pis);

	PORT_LOCK;
	uint32_t ci = AHCI_READ_4(AHCI_REG_PxCI(p->p_num));
	for (int i = 0; i < privdata->ap_ncs; i++) {
		if ((p->p_request_valid & (1 << i)) == 0)
			continue;

		/* It's valid; this could be triggered */
		if ((ci & AHCI_PxCIT_CI(i)) != 0)
			continue; /* no status update here */
		if ((p->p_request_active & (1 << i)) == 0) {
			device_printf(dev, "got trigger for inactive request %d", i);
			continue;
		}

		/*
		 * We got a transition from active -> inactive in the CI; this means
		 * the transfer is completed without errors.
		 */
		struct AHCI_PCI_REQUEST* pr = &p->p_request[i];
		struct SATA_REQUEST* sr = &pr->pr_request;
		if (sr->sr_semaphore != NULL)
			sem_signal(sr->sr_semaphore);
		if (sr->sr_bio != NULL)
			bio_set_available(sr->sr_bio);

		/* This request is no longer active nor valid */
		p->p_request_active &= ~(1 << i);
		p->p_request_valid &= ~(1 << i);
		p->p_request_in_use &= ~(1 << i);
	}
	PORT_UNLOCK;
}

static errorcode_t
ahciport_attach(device_t dev)
{
	struct AHCI_PCI_PORT* p = dev->privdata;
	struct AHCI_PCI_PRIVDATA* privdata = p->p_pd;
	int n = p->p_num;

	/*
	 * XXX We should attach ports that do not have anything connected to them;
	 *     something may be hot-plugged later
	 */
	uint32_t tfd = AHCI_READ_4(AHCI_REG_PxTFD(n));
	if ((tfd & (AHCI_PxTFD_STS_BSY | AHCI_PxTFD_STS_DRQ)) != 0) {
		AHCI_DPRINTF("skipping port #%d; bsy/drq active (%x)", n, tfd);
		return ANANAS_ERROR(NO_DEVICE);
	}
	uint32_t sts = AHCI_READ_4(AHCI_REG_PxSSTS(n));
	if (AHCI_PxSSTS_DETD(sts) != AHCI_DETD_DEV_PHY) {
		AHCI_DPRINTF("skipping port #%d; no device/phy (%x)", n, tfd);
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Initialize the DMA buffers for requests */
	for(unsigned int n = 0; n < 32; n++) {
		struct AHCI_PCI_REQUEST* pr = &p->p_request[n];
		errorcode_t err = dma_buf_alloc(dev->parent->dma_tag, sizeof(struct AHCI_PCI_CT), &pr->pr_dmabuf_ct);
		ANANAS_ERROR_RETURN(err);
		pr->pr_ct = dma_buf_get_segment(pr->pr_dmabuf_ct, 0)->s_virt;
	}

	struct DRIVER* drv = NULL;
	uint32_t sig = AHCI_READ_4(AHCI_REG_PxSIG(n));
	switch(sig) {
		case SATA_SIG_ATAPI:
			device_printf(dev, "port #%d contains packet-device, todo", n);
			break;
		case SATA_SIG_ATA:
			drv = &drv_sata_disk;
			break;
		default:
			device_printf(dev, "port #%d contains unrecognized signature %x, ignoring", n, sig);
			break;
	}

	/* There's something here; we can enable ST to queue commands to it as needed */
	AHCI_WRITE_4(AHCI_REG_PxCMD(n), AHCI_READ_4(AHCI_REG_PxCMD(n)) | AHCI_PxCMD_ST);
	if (drv != NULL) {
		/*
		 * Got a driver for this; attach it as a child to us - the port will take
		 * care of this like command queueing.
		 */
		device_t sub_dev = device_alloc(dev, drv);
		device_add_resource(sub_dev, RESTYPE_CHILDNUM, 0, 0);
		device_attach_single(sub_dev);
	}
	return ANANAS_ERROR_OK;
}

static void
ahciport_enqueue(device_t dev, void* item)
{
	struct AHCI_PCI_PORT* p = dev->privdata;
	struct AHCI_PCI_PRIVDATA* privdata = p->p_pd;

	/* Fetch an usable command slot */
	int n = -1;
	while (n < 0) {
		PORT_LOCK;
		int i = 0;
		for (/* nothing */; i < privdata->ap_ncs; i++)
			if ((p->p_request_in_use & (1 << i)) == 0)
				break;
		if (i < privdata->ap_ncs) {
			p->p_request_in_use |= 1 << i;
			n = i;
		} 
		PORT_UNLOCK;

		if (n < 0) {
			/* XXX This is valid, but it'd be odd without any load */
			device_printf(dev, "ahciport_enqueue(): out of command slots; retrying (%x)", p->p_request_in_use);

			/* XXX We should sleep on some wakeup condition - use a semaphore ? */
		}
	}

	/* Enqueue the item and mark it as valid */
	memcpy(&p->p_request[n], item, sizeof(struct SATA_REQUEST));
	PORT_LOCK;
	p->p_request_valid |= 1 << n;
	PORT_UNLOCK;
}

static void
ahciport_start(device_t dev)
{
	struct AHCI_PCI_PORT* p = dev->privdata;
	struct AHCI_PCI_PRIVDATA* privdata = p->p_pd;

	/*
	 * Program all valid requests which aren't currently active
	 *
	 * XXX Maybe we could do something more sane than keep the lock all this
	 *     time?
	 */
	PORT_LOCK;
	uint32_t ci = p->p_request_active;
	for (int i = 0; i < privdata->ap_ncs; i++) {
		if ((p->p_request_valid & (1 << i)) == 0)
			continue;
		if ((p->p_request_active & (1 << i)) != 0)
			continue;

		/* Request is valid but not yet active; program it */
		struct AHCI_PCI_REQUEST* pr = &p->p_request[i];
		struct SATA_REQUEST* sr = &pr->pr_request;

		/* XXX use dma for data_ptr */
		uint64_t data_ptr;
		if (sr->sr_buffer != NULL)
			data_ptr = kmem_get_phys(sr->sr_buffer);
		else
			data_ptr = kmem_get_phys(BIO_DATA(sr->sr_bio));

		/* Construct the command table; it will always have 1 PRD entry */
		struct AHCI_PCI_CT* ct = pr->pr_ct;
		memset(ct, 0, sizeof(struct AHCI_PCI_CT));

		ct->ct_prd[0].prde_dw0 = AHCI_PRDE_DW0_DBA(data_ptr & 0xffffffff);
		ct->ct_prd[0].prde_dw1 = AHCI_PRDE_DW1_DBAU(data_ptr >> 32);
		ct->ct_prd[0].prde_dw2 = 0;
		ct->ct_prd[0].prde_dw3 = AHCI_PRDE_DW3_I | AHCI_PRDE_DW3_DBC(sr->sr_count - 1);
		/* XXX handle atapi */
		memcpy(&ct->ct_cfis[0], &sr->sr_fis.fis_h2d, sizeof(struct SATA_FIS_H2D));
		dma_buf_sync(pr->pr_dmabuf_ct, DMA_SYNC_OUT);

		/* Set up the command list entry and hook it to this table */
		struct AHCI_PCI_CLE* cle = &p->p_cle[i];
		uint64_t addr_ct = dma_buf_get_segment(pr->pr_dmabuf_ct, 0)->s_phys;
		memset(cle, 0, sizeof(struct AHCI_PCI_CLE));
		cle->cle_dw0 =
		 AHCI_CLE_DW0_PRDTL(1) |
		 AHCI_CLE_DW0_PMP(0) |
		 AHCI_CLE_DW0_CFL(sr->sr_fis_length / 4);
		cle->cle_dw1 = 0;
		cle->cle_dw2 = AHCI_CLE_DW2_CTBA(addr_ct & 0xffffffff);
		cle->cle_dw3 = AHCI_CLE_DW3_CTBAU(addr_ct >> 32);
		dma_buf_sync(p->p_dmabuf_cl, DMA_SYNC_OUT);

		/* Command is ready to be transmitted */
		ci |= 1 << i;
	}

	/* XXXRS Maybe small race here? */
	if (ci == p->p_request_active) {
		PORT_UNLOCK;
		return;
	}
	p->p_request_active = ci;
	PORT_UNLOCK;

	AHCI_DPRINTF(">> #%d issuing command\n", p->p_num);
	DUMP_PORT_STATE(p->p_num);
	AHCI_WRITE_4(AHCI_REG_PxCI(p->p_num), ci);
}

struct DRIVER drv_ahcipci_port = {
	.name = "ahci-port",
	.drv_attach = ahciport_attach,
	.drv_enqueue = ahciport_enqueue,
	.drv_start = ahciport_start,
};

/* vim:set ts=2 sw=2: */
