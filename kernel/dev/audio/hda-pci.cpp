/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include <machine/param.h>
#include "kernel/dev/pci.h"
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/page.h"
#include "kernel/result.h"
#include "kernel/time.h"
#include "kernel/vm.h"
#include "hda.h"
#include "hda-pci.h"

#define HDA_WRITE_4(reg, val) *(volatile uint32_t*)(hda_addr + reg) = (val)
#define HDA_READ_4(reg) (*(volatile uint32_t*)(hda_addr + reg))
#define HDA_WRITE_2(reg, val) *(volatile uint16_t*)(hda_addr + reg) = (val)
#define HDA_READ_2(reg) (*(volatile uint16_t*)(hda_addr + reg))
#define HDA_WRITE_1(reg, val) *(volatile uint8_t*)(hda_addr + reg) = (val)
#define HDA_READ_1(reg) (*(volatile uint8_t*)(hda_addr + reg))

namespace hda
{
    class HDAPCIDevice : public Device,
                         private IDeviceOperations,
                         private IHDAFunctions,
                         private irq::IHandler
    {
      public:
        using Device::Device;
        virtual ~HDAPCIDevice() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

        Result IssueVerb(uint32_t verb, uint32_t* resp, uint32_t* resp_ex) override;

        uint16_t GetAndClearStateChange() override;
        Result OpenStream(int tag, int dir, uint16_t fmt, int num_pages, Context* context) override;
        Result CloseStream(Context context) override;
        Result StartStreams(int num, Context* context) override;
        Result StopStreams(int num, Context* context) override;
        void* GetStreamDataPointer(Context context, int page) override;

      private:
        irq::IRQResult OnIRQ() override;

        addr_t hda_addr;
        int hda_iss, hda_oss, hda_bss; /* stream counts, per type */
        int hda_corb_size;
        int hda_rirb_size;
        Page* hda_page;
        uint32_t* hda_corb;
        uint64_t* hda_rirb;
        int hda_rirb_rp;
        uint32_t hda_ss_avail; /* stream# available*/
        struct HDA_PCI_STREAM** hda_stream;

        HDADevice* hda_device;
    };

    irq::IRQResult HDAPCIDevice::OnIRQ()
    {
        uint32_t sts = HDA_READ_4(HDA_REG_INTSTS);
        if (sts & HDA_INTSTS_CIS) {
            uint32_t rirb_sts = HDA_READ_1(HDA_REG_RIRBSTS);
            Printf("irq; rirb sts=%x", rirb_sts);
            kprintf(
                "irq: corb rp=%x wp=%x rirb wp=%x\n", HDA_READ_2(HDA_REG_CORBRP),
                HDA_READ_2(HDA_REG_CORBWP), HDA_READ_2(HDA_REG_RIRBWP));

            HDA_WRITE_1(HDA_REG_RIRBSTS, sts & ~HDA_REG_RIRBSTS); /* acknowledge rirb status */
        }

        /* Handle stream interrupts */
        for (unsigned int n = 0; n < hda_iss + hda_oss + hda_bss; n++) {
            if ((sts & (1 << n)) == 0)
                continue;
            struct HDA_PCI_STREAM* s = hda_stream[n];

            /* Acknowledge the stream's IRQ */
            HDA_WRITE_1(
                HDA_REG_xSDnSTS(s->s_ss), HDA_READ_1(HDA_REG_xSDnSTS(s->s_ss)) | HDA_SDnSTS_BCIS);
            hda_device->OnStreamIRQ(s);
        }
        return irq::IRQResult::Processed;
    }

    Result HDAPCIDevice::IssueVerb(uint32_t verb, uint32_t* resp, uint32_t* resp_ex)
    {
        /* Ask an IRQ after 64k replies (the IRQ is masked, so we'll never see it) */
        HDA_WRITE_2(HDA_REG_RINTCNT, 0xffff);

        /* Prepare the next write pointer */
        int next_wp = (HDA_READ_2(HDA_REG_CORBWP) + 1) % hda_corb_size;
        hda_corb[next_wp] = verb;

        /* Increment the write pointer so the HDA will send the command */
        HDA_WRITE_2(HDA_REG_CORBWP, next_wp);

        /*
         * XXX For now, determine the next read pointer value and wait until it
         *     triggers... we use polling with a timeout.
         */
        int next_rp = (hda_rirb_rp + 1) % hda_rirb_size;
        int timeout = 1000;
        uint64_t r;
        for (/* nothing */; timeout > 0; timeout--) {
            uint16_t wp = HDA_READ_2(HDA_REG_RIRBWP) & 255;
            if (next_rp != wp) {
                delay(1);
                continue;
            }

            r = hda_rirb[next_rp];
            hda_rirb_rp = next_rp;
            break;
        }
        if (timeout == 0) {
            Printf("issued verb %x -> timeout", verb);
            return RESULT_MAKE_FAILURE(ENODEV); /* or another silly error code */
            ;
        }

        if (resp != NULL)
            *resp = (uint32_t)(r & 0xffffffff);
        if (resp_ex != NULL)
            *resp_ex = (uint32_t)(r >> 32);
        // HDA_DEBUG("issued verb %x -> response %x:%x", verb, (uint32_t)(r >> 32), (uint32_t)(r &
        // 0xffffffff));
        return Result::Success();
    }

    uint16_t HDAPCIDevice::GetAndClearStateChange()
    {
        uint16_t x = HDA_READ_2(HDA_REG_STATESTS);
        HDA_WRITE_2(HDA_REG_STATESTS, x & 0x7fff);
        return x;
    }

    Result HDAPCIDevice::OpenStream(int tag, int dir, uint16_t fmt, int num_pages, Context* context)
    {
        KASSERT(dir == HDF_DIR_OUT, "unsupported direction %d", dir);

        /*
         * First step is to figure out an available stream descriptor;
         * we'll start using the native ones (in/out) and revert to a
         * bi-directional if that didn't work.
         */
        int ss = -1;
        if (dir == HDF_DIR_IN) {
            /* Try input streams */
            for (int n = 0; n < hda_iss; n++) {
                if (hda_ss_avail & (1 << n)) {
                    ss = n;
                    break;
                }
            }
        } else /* dir == HDF_DIR_OUT */ {
            /* Try output streams */
            int base = hda_iss;
            for (int n = 0; n < hda_oss; n++) {
                if (hda_ss_avail & (1 << (base + n))) {
                    ss = base + n;
                    break;
                }
            }
        }

        /* If we still have found nothing, try the bidirectional streams */
        if (ss < 0) {
            int base = hda_iss + hda_oss;
            for (int n = 0; n < hda_bss; n++) {
                if (hda_ss_avail & (1 << (base + n))) {
                    ss = base + n;
                    break;
                }
            }
        }

        if (ss < 0)
            return RESULT_MAKE_FAILURE(ENOSPC); // XXX does this make sense?

        /* Claim the stream# */
        hda_ss_avail &= ~(1 << ss);

        /* Setup the stream structure */
        auto s = static_cast<struct HDA_PCI_STREAM*>(kmalloc(
            sizeof(struct HDA_PCI_STREAM) + sizeof(struct HDA_PCI_STREAM_PAGE) * num_pages));
        s->s_ss = ss;
        s->s_bdl = static_cast<struct HDA_PCI_BDL_ENTRY*>(
            page_alloc_single_mapped(s->s_bdl_page, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE));
        s->s_num_pages = num_pages;
        hda_stream[ss] = s;
        *context = s;

        /* Create a page-chain for the same data and place it in the BDL */
        struct HDA_PCI_BDL_ENTRY* bdl = s->s_bdl;
        struct HDA_PCI_STREAM_PAGE* sp = &s->s_page[0];
        for (int n = 0; n < num_pages; n++, bdl++, sp++) {
            /* XXX use dma system here */
            sp->sp_ptr = page_alloc_single_mapped(
                sp->sp_page, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE);
            bdl->bdl_addr = sp->sp_page->GetPhysicalAddress();
            bdl->bdl_length = PAGE_SIZE;
            // XXX only give IRQ on each buffer half
            bdl->bdl_flags = (n == (num_pages / 2) || n == (num_pages - 1) ? BDL_FLAG_IOC : 0);
        }

        /* All set; time to set the stream itself up */
        HDA_WRITE_4(HDA_REG_xSDnCBL(ss), num_pages * PAGE_SIZE);
        HDA_WRITE_2(HDA_REG_xSDnLVI(ss), num_pages - 1);
        HDA_WRITE_2(HDA_REG_xSDnFMT(ss), fmt);
        HDA_WRITE_4(HDA_REG_xSDnBDPL(ss), s->s_bdl_page->GetPhysicalAddress());
        HDA_WRITE_4(HDA_REG_xSDnBDPU(ss), 0); // XXX

        /* Set the stream status up - we don't set the RUN bit just yet */
        uint32_t ctl = HDA_SDnCTL_STRM(tag) | HDA_SDnCTL_DEIE | HDA_SDnCTL_FEIE | HDA_SDnCTL_IOCE;
        if (dir == HDF_DIR_IN)
            ctl |= HDA_SDnCTL_DIR_IN;
        else
            ctl |= HDA_SDnCTL_DIR_OUT;
        HDA_WRITE_4(HDA_REG_xSDnCTL(ss), ctl);
        return Result::Success();
    }

    Result HDAPCIDevice::CloseStream(void* context)
    {
        auto s = static_cast<struct HDA_PCI_STREAM*>(context);

        /* Ensure the stream is used and does not run anymore */
        KASSERT((hda_ss_avail & (1 << s->s_ss)) == 0, "closing unused stream?");
        KASSERT(
            (HDA_READ_4(HDA_REG_xSDnCTL(s->s_ss)) & HDA_SDnCTL_RUN) == 0, "closing running stream");

        /* Free all stream subpages */
        for (int n = 0; n < s->s_num_pages; n++)
            page_free(*s->s_page[n].sp_page);

        /* Free the BDL page */
        page_free(*s->s_bdl_page);

        /* Free the stream and the context */
        hda_stream[s->s_ss] = NULL;
        hda_ss_avail |= 1 << s->s_ss;
        kfree(s);

        return Result::Success();
    }

    Result HDAPCIDevice::StartStreams(int num, Context* context)
    {
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

        return Result::Success();
    }

    Result HDAPCIDevice::StopStreams(int num, void** context)
    {
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
        for (int num_stopped = 0; num_stopped < num; /* nothing */) {
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

        return Result::Success();
    }

    void* HDAPCIDevice::GetStreamDataPointer(void* context, int page)
    {
        auto s = static_cast<struct HDA_PCI_STREAM*>(context);
        KASSERT(page >= 0 && page < s->s_num_pages, "page out of range");
        return s->s_page[page].sp_ptr;
    }

    Result HDAPCIDevice::Attach()
    {
        void* res_io = d_ResourceSet.AllocateResource(Resource::RT_Memory, 4096);
        if (res_io == nullptr)
            return RESULT_MAKE_FAILURE(ENODEV);

        hda_addr = (addr_t)res_io;

        if (auto result = GetBusDeviceOperations().AllocateIRQ(*this, 0, *this); result.IsFailure())
            return result;

        /* Enable busmastering; all communication is done by DMA */
        pci_enable_busmaster(*this, 1);

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
        while (--timeout && (HDA_READ_4(HDA_REG_GCTL) & HDA_GCTL_CRST) == 0)
            delay(1);
        if (timeout == 0) {
            Printf("still stuck in reset, giving up");
            return RESULT_MAKE_FAILURE(ENODEV);
        }
        /* XXX WAKEEN ... ? */

        uint16_t gcap = HDA_READ_2(HDA_REG_GCAP);
        Printf(
            "gcap %x -> iss %d oss %d bss %d", gcap, HDA_GCAP_ISS(gcap), HDA_GCAP_OSS(gcap),
            HDA_GCAP_BSS(gcap));
        hda_iss = HDA_GCAP_ISS(gcap);
        hda_oss = HDA_GCAP_OSS(gcap);
        hda_bss = HDA_GCAP_BSS(gcap);
        if (hda_oss == 0) {
            Printf(
                "no output stream support; perhaps FIXME by implementing bss? for now, aborting");
            return RESULT_MAKE_FAILURE(ENODEV);
        }
        int num_streams = hda_iss + hda_oss + hda_bss;
        hda_stream = new HDA_PCI_STREAM*[num_streams];
        memset(hda_stream, 0, sizeof(struct HDA_PCI_STREAM*) * num_streams);

        /* Enable interrupts, also for every stream */
        uint32_t ints = 0;
        for (int n = 0; n < num_streams; n++) {
            ints = (ints << 1) | 1;
        }
        HDA_WRITE_4(HDA_REG_INTCTL, HDA_INTCTL_GIE | HDA_INTCTL_CIE | ints);

        /* Mark all stream descriptors we have as available */
        hda_ss_avail = 0;
        for (int n = 0; n < num_streams; n++)
            hda_ss_avail |= 1 << n;

        /*
         * Regardless of how large they are, the CORB and RIRB will always fit in a
         * 4KB page (max CORB size is 256 * 4 = 1KB, max RIRB size is 256 * 8 = 2KB);
         * this means we can just grab a single page use it.
         */
        static_assert(PAGE_SIZE >= 4096, "tiny page size?");
        hda_corb = static_cast<uint32_t*>(
            page_alloc_single_mapped(hda_page, VM_FLAG_READ | VM_FLAG_WRITE | VM_FLAG_DEVICE));
        hda_rirb = (uint64_t*)((char*)hda_corb + 1024);
        addr_t corb_paddr = hda_page->GetPhysicalAddress();
        memset(hda_corb, 0, PAGE_SIZE);

        /* Set up the CORB buffer; this is used to transfer commands to a codec */
        uint8_t x = HDA_READ_1(HDA_REG_CORBSIZE);
        int corb_size_val = 0;
        if (HDA_CORBSIZE_CORBSZCAP(x) & HDA_CORBSIZE_CORBSZCAP_256E) {
            hda_corb_size = 256;
            corb_size_val = HDA_CORBSIZE_CORBSIZE_256E;
        } else if (HDA_CORBSIZE_CORBSZCAP(x) & HDA_CORBSIZE_CORBSZCAP_16E) {
            hda_corb_size = 16;
            corb_size_val = HDA_CORBSIZE_CORBSIZE_16E;
        } else if (HDA_CORBSIZE_CORBSZCAP(x) & HDA_CORBSIZE_CORBSZCAP_2E) {
            hda_corb_size = 2;
            corb_size_val = HDA_CORBSIZE_CORBSIZE_2E;
        } else {
            Printf("corb size invalid?! corbsize=%x", x);
            return RESULT_MAKE_FAILURE(ENODEV);
        }
        HDA_WRITE_1(HDA_REG_CORBSIZE, (x & ~HDA_CORBSIZE_CORBSIZE_MASK) | corb_size_val);
        HDA_WRITE_4(HDA_REG_CORBL, (uint32_t)(corb_paddr & 0xffffffff));
        HDA_WRITE_4(HDA_REG_CORBU, 0); // XXX

        /* Set up the RIRB buffer; this is used to transfer responses from a codec */
        x = HDA_READ_1(HDA_REG_RIRBSIZE);
        int rirb_size_val = 0;
        if (HDA_RIRBSIZE_RIRBSZCAP(x) & HDA_RIRBSIZE_RIRBSZCAP_256E) {
            hda_rirb_size = 256;
            rirb_size_val = HDA_RIRBSIZE_RIRBSIZE_256E;
        } else if (HDA_RIRBSIZE_RIRBSZCAP(x) & HDA_RIRBSIZE_RIRBSZCAP_16E) {
            hda_rirb_size = 16;
            rirb_size_val = HDA_RIRBSIZE_RIRBSIZE_16E;
        } else if (HDA_RIRBSIZE_RIRBSZCAP(x) & HDA_RIRBSIZE_RIRBSZCAP_2E) {
            hda_rirb_size = 2;
            corb_size_val = HDA_RIRBSIZE_RIRBSIZE_2E;
        } else {
            Printf("rirb size invalid?! rirbsize=%x", x);
            return RESULT_MAKE_FAILURE(ENODEV);
        }
        HDA_WRITE_1(HDA_REG_RIRBSIZE, (x & ~HDA_RIRBSIZE_RIRBSIZE_MASK) | rirb_size_val);
        HDA_WRITE_4(HDA_REG_RIRBL, (uint32_t)((corb_paddr + 1024) & 0xffffffff));
        HDA_WRITE_4(HDA_REG_RIRBU, 0); // XXX
        hda_rirb_rp = 0;

        /* Kick the CORB and RIRB into action */
        HDA_WRITE_1(
            HDA_REG_CORBCTL,
            HDA_READ_1(HDA_REG_CORBCTL) | (HDA_CORBCTL_CORBRUN | HDA_CORBCTL_CMEIE));
        HDA_WRITE_1(
            HDA_REG_RIRBCTL,
            HDA_READ_1(HDA_REG_RIRBCTL) | (HDA_RIRBCTL_RIRBDMAEN /* | HDA_RIRBCTL_RINTCTL */));

        // Hook up the HDA device on top of us
        hda_device = static_cast<hda::HDADevice*>(
            device_manager::CreateDevice("hda", CreateDeviceProperties(*this, ResourceSet())));
        KASSERT(hda_device != nullptr, "unable to create hda device");
        hda_device->SetHDAFunctions(*this);

        if (auto result = device_manager::AttachSingle(*hda_device); result.IsFailure()) {
            /* XXX we should clean up the tree thus far */
            page_free(*hda_page);
            return result;
        }

        return Result::Success();
    }

    Result HDAPCIDevice::Detach()
    {
        panic("detach");
        return Result::Success();
    }

    namespace
    {
        struct HDAPCI_Driver : public Driver {
            HDAPCI_Driver() : Driver("hda-pci") {}

            const char* GetBussesToProbeOn() const override { return "pcibus"; }

            Device* CreateDevice(const CreateDeviceProperties& cdp) override
            {
                auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PCI_VendorID, 0);
                if (res == nullptr)
                    return nullptr; // XXX this should be fixed; attach_bus will try the entire
                                    // attach-cycle without PCI resources

                uint32_t vendor = res->r_Base;
                res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PCI_DeviceID, 0);
                uint32_t device = res->r_Base;
                if (vendor == 0x8086 && device == 0x2668) /* intel hda in QEMU */
                    return new HDAPCIDevice(cdp);
                if (vendor == 0x10de && device == 0x7fc) /* nvidia MCP73 HDA */
                    return new HDAPCIDevice(cdp);
                if (vendor == 0x1022 && device == 0x780d) /* AMD FCH Azalia Controller */
                    return new HDAPCIDevice(cdp);
                if (vendor == 0x1039 && device == 0x7502) /* SiS 966 */
                    return new HDAPCIDevice(cdp);
                return nullptr;
            }
        };

        const RegisterDriver<HDAPCI_Driver> registerDriver;

    } // unnamed namespace
} // namespace hda
