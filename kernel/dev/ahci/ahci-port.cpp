#include <ananas/types.h>
#include <ananas/error.h>
#include "kernel/bio.h"
#include "kernel/dev/sata.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/kmem.h"
#include "kernel/lib.h"
#include "kernel/trace.h"
#include "ahci.h"
#include "ahci-pci.h"

TRACE_SETUP;

#define PORT_LOCK spinlock_lock(p_lock)
#define PORT_UNLOCK spinlock_unlock(p_lock)

namespace Ananas {
namespace AHCI {

Port::Port(const Ananas::CreateDeviceProperties& cdp)
	: Device(cdp), p_device(static_cast<AHCIDevice&>(*cdp.cdp_Parent))
{
	p_request_in_use = 0;
	p_request_active = 0;
	p_request_valid = 0;
	memset(&p_request, 0, sizeof(p_request));
}

void
Port::OnIRQ(uint32_t pis)
{
	AHCI_DPRINTF("got irq, pis=%x", pis);

	PORT_LOCK;
	uint32_t ci = p_device.Read(AHCI_REG_PxCI(p_num));
	for (int i = 0; i < p_device.ap_ncs; i++) {
		if ((p_request_valid & (1 << i)) == 0)
			continue;

		/* It's valid; this could be triggered */
		if ((ci & AHCI_PxCIT_CI(i)) != 0)
			continue; /* no status update here */
		if ((p_request_active & (1 << i)) == 0) {
			Printf("got trigger for inactive request %d", i);
			continue;
		}

		/*
		 * We got a transition from active -> inactive in the CI; this means
		 * the transfer is completed without errors.
		 */
		Request* pr = &p_request[i];
		struct SATA_REQUEST* sr = &pr->pr_request;
		if (sr->sr_semaphore != nullptr)
			sem_signal(*sr->sr_semaphore);
		if (sr->sr_bio != nullptr) {
			if (sr->sr_flags & SATA_REQUEST_FLAG_WRITE)
				sr->sr_bio->flags &= ~BIO_FLAG_DIRTY;
			bio_set_available(*sr->sr_bio);
		}

		/* This request is no longer active nor valid */
		p_request_active &= ~(1 << i);
		p_request_valid &= ~(1 << i);
		p_request_in_use &= ~(1 << i);
	}
	PORT_UNLOCK;
}

errorcode_t
Port::Attach()
{
	int n = p_num;

	/*
	 * XXX We should attach ports that do not have anything connected to them;
	 *     something may be hot-plugged later
	 */
	uint32_t tfd = p_device.Read(AHCI_REG_PxTFD(n));
	if ((tfd & (AHCI_PxTFD_STS_BSY | AHCI_PxTFD_STS_DRQ)) != 0) {
		AHCI_DPRINTF("skipping port #%d; bsy/drq active (%x)", n, tfd);
		return ANANAS_ERROR(NO_DEVICE);
	}
	uint32_t sts = p_device.Read(AHCI_REG_PxSSTS(n));
	if (AHCI_PxSSTS_DETD(sts) != AHCI_DETD_DEV_PHY) {
		AHCI_DPRINTF("skipping port #%d; no device/phy (%x)", n, tfd);
		return ANANAS_ERROR(NO_DEVICE);
	}

	/* Initialize the DMA buffers for requests */
	for(unsigned int n = 0; n < 32; n++) {
		Request* pr = &p_request[n];
		errorcode_t err = dma_buf_alloc(p_device.d_DMA_tag, sizeof(struct AHCI_PCI_CT), &pr->pr_dmabuf_ct);
		ANANAS_ERROR_RETURN(err);
		pr->pr_ct = static_cast<struct AHCI_PCI_CT*>(dma_buf_get_segment(pr->pr_dmabuf_ct, 0)->s_virt);
	}

	Ananas::Device* sub_device = nullptr;
	Ananas::ResourceSet sub_resourceSet;
	sub_resourceSet.AddResource(Ananas::Resource(Ananas::Resource::RT_ChildNum, 0, 0));

	uint32_t sig = p_device.Read(AHCI_REG_PxSIG(n));
	switch(sig) {
		case SATA_SIG_ATAPI:
			Printf("port #%d contains packet-device, todo", n);
			break;
		case SATA_SIG_ATA:
			sub_device = Ananas::DeviceManager::CreateDevice("satadisk", Ananas::CreateDeviceProperties(*this, sub_resourceSet));
			break;
		default:
			Printf("port #%d contains unrecognized signature %x, ignoring", n, sig);
			break;
	}

	/* There's something here; we can enable ST to queue commands to it as needed */
	p_device.Write(AHCI_REG_PxCMD(n), p_device.Read(AHCI_REG_PxCMD(n)) | AHCI_PxCMD_ST);
	if (sub_device != nullptr) {
		/*
		 * Got a driver for this; attach it as a child to us - the port will take
		 * care of this like command queueing.
		 */
		Ananas::DeviceManager::AttachSingle(*sub_device);
	}
	return ananas_success();
}

errorcode_t
Port::Detach()
{
	panic("TODO");
	return ananas_success();
}

void
Port::Enqueue(void* item)
{
	/* Fetch an usable command slot */
	int n = -1;
	while (n < 0) {
		PORT_LOCK;
		int i = 0;
		for (/* nothing */; i < p_device.ap_ncs; i++)
			if ((p_request_in_use & (1 << i)) == 0)
				break;
		if (i < p_device.ap_ncs) {
			p_request_in_use |= 1 << i;
			n = i;
		} 
		PORT_UNLOCK;

		if (n < 0) {
			/* XXX This is valid, but it'd be odd without any load */
			Printf("ahciport_enqueue(): out of command slots; retrying (%x)", p_request_in_use);

			/* XXX We should sleep on some wakeup condition - use a semaphore ? */
		}
	}

	/* Enqueue the item and mark it as valid */
	memcpy(&p_request[n], item, sizeof(struct SATA_REQUEST));
	PORT_LOCK;
	p_request_valid |= 1 << n;
	PORT_UNLOCK;
}

void
Port::Start()
{
	/*
	 * Program all valid requests which aren't currently active
	 *
	 * XXX Maybe we could do something more sane than keep the lock all this
	 *     time?
	 */
	PORT_LOCK;
	uint32_t ci = p_request_active;
	for (int i = 0; i < p_device.ap_ncs; i++) {
		if ((p_request_valid & (1 << i)) == 0)
			continue;
		if ((p_request_active & (1 << i)) != 0)
			continue;

		/* Request is valid but not yet active; program it */
		Request* pr = &p_request[i];
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
		ct->ct_prd[0].prde_dw3 = AHCI_PRDE_DW3_DBC(sr->sr_count - 1);
		/* XXX handle atapi */
		memcpy(&ct->ct_cfis[0], &sr->sr_fis.fis_h2d, sizeof(struct SATA_FIS_H2D));
		dma_buf_sync(pr->pr_dmabuf_ct, DMA_SYNC_OUT);

		/* Set up the command list entry and hook it to this table */
		struct AHCI_PCI_CLE* cle = &p_cle[i];
		uint64_t addr_ct = dma_buf_get_segment(pr->pr_dmabuf_ct, 0)->s_phys;
		memset(cle, 0, sizeof(struct AHCI_PCI_CLE));
		cle->cle_dw0 =
		 AHCI_CLE_DW0_PRDTL(1) |
		 AHCI_CLE_DW0_PMP(0) |
		 AHCI_CLE_DW0_CFL(sr->sr_fis_length / 4);
		if (sr->sr_flags & SATA_REQUEST_FLAG_WRITE)
			cle->cle_dw0 |= AHCI_CLE_DW0_W;
		cle->cle_dw1 = 0;
		cle->cle_dw2 = AHCI_CLE_DW2_CTBA(addr_ct & 0xffffffff);
		cle->cle_dw3 = AHCI_CLE_DW3_CTBAU(addr_ct >> 32);
		dma_buf_sync(p_dmabuf_cl, DMA_SYNC_OUT);

		/* Command is ready to be transmitted */
		ci |= 1 << i;
	}

	/* XXXRS Maybe small race here? */
	if (ci == p_request_active) {
		PORT_UNLOCK;
		return;
	}
	p_request_active = ci;
	PORT_UNLOCK;

	AHCI_DPRINTF(">> #%d issuing command\n", p_num);
	DUMP_PORT_STATE(p_num);

	p_device.Write(AHCI_REG_PxCI(p_num), ci);
}

} // namespace AHCI
} // namespace Ananas

namespace {

struct AHCI_Port_Driver : public Ananas::Driver
{
	AHCI_Port_Driver()
	 : Driver("ahci-port")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return nullptr; // created by ahci-pci for every port detected
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		return new Ananas::AHCI::Port(cdp);
	}
};

} // unnamed namespace

REGISTER_DRIVER(AHCI_Port_Driver)

/* vim:set ts=2 sw=2: */
