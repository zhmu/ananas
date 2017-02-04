#include <ananas/types.h>
#include <ananas/dev/hda.h>
#include <ananas/dev/hda-pci.h>
#include <ananas/bus/pci.h>
#include <ananas/device.h>
#include <ananas/error.h>
#include <ananas/irq.h>
#include <ananas/page.h>
#include <ananas/time.h>
#include <ananas/mm.h>
#include <ananas/trace.h>
#include <ananas/lib.h>
#include <ananas/vm.h>
#include <machine/param.h>

TRACE_SETUP;

#define HDA_WRITE_4(reg, val) \
	*(volatile uint32_t*)(privdata->hda_addr + reg) = (val)
#define HDA_READ_4(reg) \
	(*(volatile uint32_t*)(privdata->hda_addr + reg))
#define HDA_WRITE_2(reg, val) \
	*(volatile uint16_t*)(privdata->hda_addr + reg) = (val)
#define HDA_READ_2(reg) \
	(*(volatile uint16_t*)(privdata->hda_addr + reg))
#define HDA_WRITE_1(reg, val) \
	*(volatile uint8_t*)(privdata->hda_addr + reg) = (val)
#define HDA_READ_1(reg) \
	(*(volatile uint8_t*)(privdata->hda_addr + reg))

static void
hdapci_stream_irq(device_t dev, struct HDA_PCI_STREAM* s)
{
	struct HDA_PRIVDATA* hda_pd = dev->privdata;
	struct HDA_PCI_PRIVDATA* privdata = hda_pd->hda_dev_priv;

	/* Acknowledge the stream's IRQ */
	HDA_WRITE_1(HDA_REG_xSDnSTS(s->s_ss), HDA_READ_1(HDA_REG_xSDnSTS(s->s_ss)) | HDA_SDnSTS_BCIS);

	hda_stream_irq(dev, s);
}


static irqresult_t
hdapci_irq(device_t dev, void* context)
{
	struct HDA_PCI_PRIVDATA* privdata = context;

	uint32_t sts = HDA_READ_4(HDA_REG_INTSTS);
	if (sts & HDA_INTSTS_CIS) {
		uint32_t rirb_sts = HDA_READ_1(HDA_REG_RIRBSTS);
		device_printf(dev, "irq; rirb sts=%x", rirb_sts);
		kprintf("irq: corb rp=%x wp=%x rirb wp=%x\n", HDA_READ_2(HDA_REG_CORBRP), HDA_READ_2(HDA_REG_CORBWP), HDA_READ_2(HDA_REG_RIRBWP));

		HDA_WRITE_1(HDA_REG_RIRBSTS, sts & ~HDA_REG_RIRBSTS); /* acknowledge rirb status */
	}

	/* Handle stream interrupts */
	for (unsigned int n = 0; n < privdata->hda_iss + privdata->hda_oss + privdata->hda_bss; n++) {
		if ((sts & (1 << n)) == 0)
			continue;
		hdapci_stream_irq(dev, privdata->hda_stream[n]);
	}

	return IRQ_RESULT_PROCESSED;
}

static errorcode_t
hdapci_probe(device_t dev)
{
	struct RESOURCE* res = device_get_resource(dev, RESTYPE_PCI_VENDORID, 0);
	if (res == NULL)
		return ANANAS_ERROR(NO_RESOURCE); // XXX this should be fixed; attach_bus will try the entire attach-cycle without PCI resources!

	uint32_t vendor = res->base;
	res = device_get_resource(dev, RESTYPE_PCI_DEVICEID, 0);
	uint32_t device = res->base;
	if (vendor == 0x8086 && device == 0x2668) /* intel hda in QEMU */
		return ananas_success();
	if (vendor == 0x10de && device == 0x7fc) /* nvidia MCP73 HDA */
		return ananas_success();
	if (vendor == 0x1039 && device == 0x7502) /* SiS 966 */
		return ananas_success();
	return ANANAS_ERROR(NO_RESOURCE);
}

static errorcode_t
hdapci_issue_verb(device_t dev, uint32_t verb, uint32_t* resp, uint32_t* resp_ex)
{
	struct HDA_PRIVDATA* hda_pd = dev->privdata;
	struct HDA_PCI_PRIVDATA* privdata = hda_pd->hda_dev_priv;

	/* Ask an IRQ after 64k replies (the IRQ is masked, so we'll never see it) */
	HDA_WRITE_2(HDA_REG_RINTCNT, 0xffff);

	/* Prepare the next write pointer */
	int next_wp = (HDA_READ_2(HDA_REG_CORBWP) + 1) % privdata->hda_corb_size;
	privdata->hda_corb[next_wp] = verb;

	/* Increment the write pointer so the HDA will send the command */
	HDA_WRITE_2(HDA_REG_CORBWP, next_wp);

	/*
	 * XXX For now, determine the next read pointer value and wait until it
	 *     triggers... we use polling with a timeout.
	 */
	int next_rp = (privdata->hda_rirb_rp + 1) % privdata->hda_rirb_size;
	int timeout = 1000;
	uint64_t r;
	for(/* nothing */; timeout > 0; timeout--) {
		uint16_t wp = HDA_READ_2(HDA_REG_RIRBWP) & 255;
		if (next_rp != wp) {
			delay(1);
			continue;
		}

		r = privdata->hda_rirb[next_rp];
		privdata->hda_rirb_rp = next_rp;
		break;
	}
	if (timeout == 0) {
		HDA_DEBUG("issued verb %x -> timeout", verb);
		return ANANAS_ERROR(NO_DEVICE); /* or another silly error code */
	}

	if (resp != NULL)
		*resp = (uint32_t)(r & 0xffffffff);
	if (resp_ex != NULL)
		*resp_ex = (uint32_t)(r >> 32);
	//HDA_DEBUG("issued verb %x -> response %x:%x", verb, (uint32_t)(r >> 32), (uint32_t)(r & 0xffffffff));
	return ananas_success();
}

static uint16_t
hdapci_get_state_change(device_t dev)
{
	struct HDA_PRIVDATA* hda_pd = dev->privdata;
	struct HDA_PCI_PRIVDATA* privdata = hda_pd->hda_dev_priv;

	uint16_t x = HDA_READ_2(HDA_REG_STATESTS);
	HDA_WRITE_2(HDA_REG_STATESTS, x & 0x7fff);
	return x;
}

static errorcode_t
hdapci_open_stream(device_t dev, int tag, int dir, uint16_t fmt, int num_pages, void** context)
{
	struct HDA_PRIVDATA* hda_pd = dev->privdata;
	struct HDA_PCI_PRIVDATA* privdata = hda_pd->hda_dev_priv;

	KASSERT(dir == HDF_DIR_OUT, "unsupported direction %d", dir);

	/*
	 * First step is to figure out an available stream descriptor;
	 * we'll start using the native ones (in/out) and revert to a
	 * bi-directional if that didn't work.
	 */
	int ss = -1;
	if (dir == HDF_DIR_IN) {
		/* Try input streams */
		for (int n = 0; n < privdata->hda_iss; n++) {
			if (privdata->hda_ss_avail & (1 << n)) {
				ss = n;
				break;
			}
		}
	} else /* dir == HDF_DIR_OUT */ {
		/* Try output streams */
		int base = privdata->hda_iss;
		for (int n = 0; n < privdata->hda_oss; n++) {
			if (privdata->hda_ss_avail & (1 << (base + n))) {
				ss = base + n;
				break;
			}
		}
	}

	/* If we still have found nothing, try the bidirectional streams */
	if (ss < 0) {
		int base = privdata->hda_iss + privdata->hda_oss;
		for (int n = 0; n < privdata->hda_bss; n++) {
			if (privdata->hda_ss_avail & (1 << (base + n))) {
				ss = base + n;
				break;
			}
		}
	}

	if (ss < 0)
		return ANANAS_ERROR(NO_SPACE);

	/* Claim the stream# */
	privdata->hda_ss_avail &= ~(1 << ss);

	/* Setup the stream structure */
	struct HDA_PCI_STREAM* s = kmalloc(sizeof *s + sizeof(struct HDA_PCI_STREAM_PAGE) * num_pages);
	s->s_ss = ss;
	s->s_bdl = page_alloc_single_mapped(&s->s_bdl_page, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE);
	s->s_num_pages = num_pages;
	privdata->hda_stream[ss] = s;
	*context = s;

	/* Create a page-chain for the same data and place it in the BDL */
	struct HDA_PCI_BDL_ENTRY* bdl = s->s_bdl;
	struct HDA_PCI_STREAM_PAGE* sp = &s->s_page[0];
	for (int n = 0; n < num_pages; n++, bdl++, sp++) {
		/* XXX use dma system here */
		sp->sp_ptr = page_alloc_single_mapped(&sp->sp_page, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE);
		bdl->bdl_addr = page_get_paddr(sp->sp_page);
		bdl->bdl_length = PAGE_SIZE;
		// XXX only give IRQ on each buffer half
		bdl->bdl_flags = (n == (num_pages/ 2) || n == (num_pages - 1) ? BDL_FLAG_IOC : 0);
	}

	/* All set; time to set the stream itself up */
	HDA_WRITE_4(HDA_REG_xSDnCBL(ss), num_pages * PAGE_SIZE);
	HDA_WRITE_4(HDA_REG_xSDnLVI(ss), num_pages - 1);
	HDA_WRITE_2(HDA_REG_xSDnFMT(ss), fmt);
	HDA_WRITE_4(HDA_REG_xSDnBDPL(ss), page_get_paddr(s->s_bdl_page));
	HDA_WRITE_4(HDA_REG_xSDnBDPU(ss), 0); // XXX

	/* Set the stream status up - we don't set the RUN bit just yet */
	uint32_t ctl = HDA_SDnCTL_STRM(tag) | HDA_SDnCTL_DEIE | HDA_SDnCTL_FEIE | HDA_SDnCTL_IOCE;
	if (dir == HDF_DIR_IN)
		ctl |= HDA_SDnCTL_DIR_IN;
	else
		ctl |= HDA_SDnCTL_DIR_OUT;
	HDA_WRITE_4(HDA_REG_xSDnCTL(ss), ctl);
	return ananas_success();
}

static errorcode_t
hdapci_close_stream(device_t dev, void* context)
{
	struct HDA_PRIVDATA* hda_pd = dev->privdata;
	struct HDA_PCI_PRIVDATA* privdata = hda_pd->hda_dev_priv;
	struct HDA_PCI_STREAM* s = context;

	/* Ensure the stream is used and does not run anymore */
	KASSERT((privdata->hda_ss_avail & (1 << s->s_ss)) == 0, "closing unused stream?");
	KASSERT((HDA_READ_4(HDA_REG_xSDnCTL(s->s_ss)) & HDA_SDnCTL_RUN) == 0, "closing running stream");

	/* Free all stream subpages */
	for (int n = 0; n < s->s_num_pages; n++)
		page_free(s->s_page[n].sp_page);

	/* Free the BDL page */
	page_free(s->s_bdl_page);

	/* Free the stream and the context */
	privdata->hda_stream[s->s_ss] = NULL;
	privdata->hda_ss_avail |= 1 << s->s_ss;
	kfree(s);

	return ananas_success();
}

static errorcode_t
hdapci_start_streams(device_t dev, int num, void** context)
{
	struct HDA_PRIVDATA* hda_pd = dev->privdata;
	struct HDA_PCI_PRIVDATA* privdata = hda_pd->hda_dev_priv;
	struct HDA_PCI_STREAM** s;

	/*
	 * XXX No one seems to use the 'synchronized play'-stuff, but the spec is
	 *     clear on it - it works on actual hardware but not in VirtualBox/QEMU,
	 *     so let's just ignore it for now...
	 */
#if 0
	/* First step is to set all SSYNC bits */
	uint32_t ssync = HDA_READ_4(HDA_REG_SSYNC);
	s = (struct HDA_PCI_STREAM**)context;
	for (int n = 0; n < num; n++, s++) {
		ssync |= 1 << (*s)->s_ss;
	}
	HDA_WRITE_4(HDA_REG_SSYNC, ssync);
#endif

	/* Now set the running bits of all streams in place */
	s = (struct HDA_PCI_STREAM**)context;
	for (int n = 0; n < num; n++, s++) {
		uint32_t ctl = HDA_READ_4(HDA_REG_xSDnCTL((*s)->s_ss));
		HDA_WRITE_4(HDA_REG_xSDnCTL((*s)->s_ss), ctl | HDA_SDnCTL_RUN);
	}

#if 0
	/* Wait until each stream's FIFO has been filled */
	for(int fifo_ready = 0; fifo_ready < num; /* nothing */) {
		fifo_ready = 0;
		s = (struct HDA_PCI_STREAM**)context;
		for (int n = 0; n < num; n++, s++)
			if (HDA_READ_4(HDA_REG_xSDnSTS((*s)->s_ss)) & HDA_SDnSTS_FIFORDY)
				fifo_ready++;
	}

	/* We can now enable all streams in a single go */
	ssync = HDA_READ_4(HDA_REG_SSYNC);
	s = (struct HDA_PCI_STREAM**)context;
	for (int n = 0; n < num; n++, s++) {
		ssync &= ~(1 << (*s)->s_ss);
	}
	HDA_WRITE_4(HDA_REG_SSYNC, ssync);
#endif

	return ananas_success();
}

static errorcode_t
hdapci_stop_streams(device_t dev, int num, void** context)
{
	struct HDA_PRIVDATA* hda_pd = dev->privdata;
	struct HDA_PCI_PRIVDATA* privdata = hda_pd->hda_dev_priv;

	/* First step is to set all SSYNC bits to prevent the HDA from fetching new data */
	uint32_t ssync = HDA_READ_4(HDA_REG_SSYNC);
	struct HDA_PCI_STREAM** s = (struct HDA_PCI_STREAM**)context;
	for (int n = 0; n < num; n++, s++) {
		ssync |= 1 << (*s)->s_ss;
	}
	HDA_WRITE_4(HDA_REG_SSYNC, ssync);

	/* Clear the running bits of all streams in place; they will finish the current run */
	s = (struct HDA_PCI_STREAM**)context;
	for (int n = 0; n < num; n++, s++) {
		uint32_t ctl = HDA_READ_4(HDA_REG_xSDnCTL((*s)->s_ss));
		HDA_WRITE_4(HDA_REG_xSDnCTL((*s)->s_ss), ctl & ~HDA_SDnCTL_RUN);
	}

	/* Wait until all streams are stopped; we can then clear the SSYNC bit we set */
	for(int num_stopped = 0; num_stopped < num; /* nothing */) {
		s = (struct HDA_PCI_STREAM**)context;
		for (int n = 0; n < num; n++, s++)
			if ((HDA_READ_4(HDA_REG_xSDnCTL((*s)->s_ss)) & HDA_SDnCTL_RUN) == 0)
				num_stopped++;
	}

	/* Everything is stopped; clear the SSYNC bits and we are done */
	ssync = HDA_READ_4(HDA_REG_SSYNC);
	s = (struct HDA_PCI_STREAM**)context;
	for (int n = 0; n < num; n++, s++) {
		ssync &= ~(1 << (*s)->s_ss);
	}
	HDA_WRITE_4(HDA_REG_SSYNC, ssync);

	return ananas_success();
}

static void*
hdapci_get_stream_data_ptr(device_t dev, void* context, int page)
{
	struct HDA_PCI_STREAM* s = context;
	KASSERT(page >= 0 && page < s->s_num_pages, "page out of range");
	return s->s_page[page].sp_ptr;
}

static struct HDA_DEV_FUNC hdapci_devfuncs = {
	.hdf_issue_verb = hdapci_issue_verb,
	.hdf_get_state_change = hdapci_get_state_change,
	.hdf_open_stream = hdapci_open_stream,
	.hdf_close_stream = hdapci_close_stream,
	.hdf_start_streams = hdapci_start_streams,
	.hdf_stop_streams = hdapci_stop_streams,
	.hdf_get_stream_data_ptr = hdapci_get_stream_data_ptr,
};

static errorcode_t
hdapci_attach(device_t dev)
{
	void* res_io = device_alloc_resource(dev, RESTYPE_MEMORY, 4096);
	void* res_irq = device_alloc_resource(dev, RESTYPE_IRQ, 0);
	if (res_io == NULL || res_irq == NULL)
		return ANANAS_ERROR(NO_RESOURCE);

	struct HDA_PCI_PRIVDATA* privdata = kmalloc(sizeof(struct HDA_PCI_PRIVDATA));
	memset(privdata, 0, sizeof(*privdata));
	privdata->hda_addr = (addr_t)res_io;

	errorcode_t err = irq_register((uintptr_t)res_irq, dev, hdapci_irq, IRQ_TYPE_DEFAULT, privdata);
	ANANAS_ERROR_RETURN(err);

	/* Enable busmastering; all communication is done by DMA */
	pci_enable_busmaster(dev, 1);

	/*
	 * We have a memory address and IRQ for the HDA controller; we now need to
	 * properly initialize it; reset the device and wait a short while for it to
	 * become available.
	 */
	HDA_WRITE_4(HDA_REG_GCTL, HDA_READ_4(HDA_REG_GCTL) & ~HDA_GCTL_CRST);
	delay(1);
	HDA_WRITE_4(HDA_REG_GCTL, HDA_READ_4(HDA_REG_GCTL) | HDA_GCTL_CRST);
	delay(1);
	int timeout = 100;
	while(--timeout && (HDA_READ_4(HDA_REG_GCTL) & HDA_GCTL_CRST) == 0)
		delay(1);
	if (timeout == 0) {
		kfree(privdata);
		device_printf(dev, "still stuck in reset, giving up");
		return ANANAS_ERROR(NO_DEVICE);
	}
	/* XXX WAKEEN ... ? */

	uint16_t gcap = HDA_READ_2(HDA_REG_GCAP);
	device_printf(dev, "gcap %x -> iss %d oss %d bss %d",
	 gcap,
	 HDA_GCAP_ISS(gcap),
	 HDA_GCAP_OSS(gcap),
	 HDA_GCAP_BSS(gcap));
	privdata->hda_iss = HDA_GCAP_ISS(gcap);
	privdata->hda_oss = HDA_GCAP_OSS(gcap);
	privdata->hda_bss = HDA_GCAP_BSS(gcap);
	if (privdata->hda_oss == 0) {
		kfree(privdata);
		device_printf(dev, "no output stream support; perhaps FIXME by implementing bss? for now, aborting");
		return ANANAS_ERROR(NO_DEVICE);
	}
	int num_streams = privdata->hda_iss + privdata->hda_oss + privdata->hda_bss;
	privdata->hda_stream = kmalloc(sizeof(struct HDA_PCI_STREAM*) * num_streams);
	memset(privdata->hda_stream, 0, sizeof(struct HDA_PCI_STREAM*) * num_streams);

	/* Enable interrupts, also for every stream */
	uint32_t ints = 0;
	for (int n = 0; n < num_streams; n++) {
		ints = (ints << 1) | 1;
	}
	HDA_WRITE_4(HDA_REG_INTCTL, HDA_INTCTL_GIE | HDA_INTCTL_CIE | ints);

	/* Mark all stream descriptors we have as available */
	privdata->hda_ss_avail = 0;
	for (int n = 0; n < num_streams; n++)
		privdata->hda_ss_avail |= 1 << n;

	/*
	 * Regardless of how large they are, the CORB and RIRB will always fit in a
	 * 4KB page (max CORB size is 256 * 4 = 1KB, max RIRB size is 256 * 8 = 2KB);
	 * this means we can just grab a single page use it.
	 */
	KASSERT(PAGE_SIZE >= 4096, "tiny page size?"); /* XXX could be a static assert */
	privdata->hda_corb = page_alloc_single_mapped(&privdata->hda_page, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE);
	privdata->hda_rirb = (uint64_t*)((char*)privdata->hda_corb + 1024);
	addr_t corb_paddr = page_get_paddr(privdata->hda_page);
	memset(privdata->hda_corb, 0, PAGE_SIZE);

	/* Set up the CORB buffer; this is used to transfer commands to a codec */
	uint8_t x = HDA_READ_1(HDA_REG_CORBSIZE);
	int corb_size_val = 0;
	if (HDA_CORBSIZE_CORBSZCAP(x) & HDA_CORBSIZE_CORBSZCAP_256E) {
		privdata->hda_corb_size = 256; corb_size_val = HDA_CORBSIZE_CORBSIZE_256E;
	} else if (HDA_CORBSIZE_CORBSZCAP(x) & HDA_CORBSIZE_CORBSZCAP_16E) {
		privdata->hda_corb_size = 16; corb_size_val = HDA_CORBSIZE_CORBSIZE_16E;
	} else if (HDA_CORBSIZE_CORBSZCAP(x) & HDA_CORBSIZE_CORBSZCAP_2E) {
		privdata->hda_corb_size = 2; corb_size_val = HDA_CORBSIZE_CORBSIZE_2E;
	} else {
		device_printf(dev, "corb size invalid?! corbsize=%x", x);
		return ANANAS_ERROR(NO_DEVICE);
	}
	HDA_WRITE_1(HDA_REG_CORBSIZE, (x & ~HDA_CORBSIZE_CORBSIZE_MASK) | corb_size_val);
	HDA_WRITE_4(HDA_REG_CORBL, (uint32_t)(corb_paddr & 0xffffffff));
	HDA_WRITE_4(HDA_REG_CORBU, 0); // XXX

	/* Set up the RIRB buffer; this is used to transfer responses from a codec */
	x = HDA_READ_1(HDA_REG_RIRBSIZE);
	int rirb_size_val = 0;
	if (HDA_RIRBSIZE_RIRBSZCAP(x) & HDA_RIRBSIZE_RIRBSZCAP_256E) {
		privdata->hda_rirb_size = 256; rirb_size_val = HDA_RIRBSIZE_RIRBSIZE_256E;
	} else if (HDA_RIRBSIZE_RIRBSZCAP(x) & HDA_RIRBSIZE_RIRBSZCAP_16E) {
		privdata->hda_rirb_size = 16; rirb_size_val = HDA_RIRBSIZE_RIRBSIZE_16E;
	} else if (HDA_RIRBSIZE_RIRBSZCAP(x) & HDA_RIRBSIZE_RIRBSZCAP_2E) {
		privdata->hda_rirb_size = 2; corb_size_val = HDA_RIRBSIZE_RIRBSIZE_2E;
	} else {
		device_printf(dev, "rirb size invalid?! rirbsize=%x", x);
		return ANANAS_ERROR(NO_DEVICE);
	}
	HDA_WRITE_1(HDA_REG_RIRBSIZE, (x & ~HDA_RIRBSIZE_RIRBSIZE_MASK) | rirb_size_val);
	HDA_WRITE_4(HDA_REG_RIRBL, (uint32_t)((corb_paddr + 1024) & 0xffffffff));
	HDA_WRITE_4(HDA_REG_RIRBU, 0); // XXX
	privdata->hda_rirb_rp = 0;

	/* Kick the CORB and RIRB into action */
	HDA_WRITE_1(HDA_REG_CORBCTL, HDA_READ_1(HDA_REG_CORBCTL) | (HDA_CORBCTL_CORBRUN | HDA_CORBCTL_CMEIE));
	HDA_WRITE_1(HDA_REG_RIRBCTL, HDA_READ_1(HDA_REG_RIRBCTL) | (HDA_RIRBCTL_RIRBDMAEN /* | HDA_RIRBCTL_RINTCTL */));

	err = hda_attach(dev, &hdapci_devfuncs, privdata);
	if (ananas_is_success(err))
		return err;

	/* XXX we should clean up the tree thus far */
	page_free(privdata->hda_page);
	kfree(privdata);
	return err;
}

struct DRIVER drv_hdapci = {
	.name					= "hdapci",
	.drv_probe		= hdapci_probe,
	.drv_attach		= hdapci_attach,
};

DRIVER_PROBE(hdapci)
DRIVER_PROBE_BUS(pcibus)
DRIVER_PROBE_END()

/* vim:set ts=2 sw=2: */
