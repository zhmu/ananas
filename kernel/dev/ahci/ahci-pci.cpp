#include <ananas/types.h>
#include "kernel/dev/pci.h"
#include "kernel/dev/sata.h"
#include "kernel/device.h"
#include "kernel/dma.h"
#include "kernel/driver.h"
#include "kernel/irq.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/time.h"
#include "kernel/trace.h"
#include "kernel/vm.h"
#include "kernel-md/dma.h"
#include "kernel-md/vm.h"
#include "ahci.h"
#include "ahci-pci.h"

TRACE_SETUP;

namespace ahci
{
    irq::IRQResult AHCIDevice::OnIRQ()
    {
        uint32_t is = Read(AHCI_REG_IS);
        AHCI_DPRINTF("irq: is=%x", is);

        for (unsigned int n = 0; n < 32; n++) {
            if ((is & AHCI_IS_IPS(n)) == 0)
                continue;

            /* XXX We could directly look them up */
            Port* p = NULL;
            for (unsigned int i = 0; i < ap_num_ports; i++)
                if (ap_port[i]->p_num == n) {
                    p = ap_port[i];
                    break;
                }

            uint32_t pis = Read(AHCI_REG_PxIS(n));
            Write(AHCI_REG_PxIS(n), pis);
            if (p != NULL) {
                p->OnIRQ(pis);
            } else {
                AHCI_DPRINTF("got IRQ for unsupported port %d", n);
            }
        }
        Write(AHCI_REG_IS, is);
        return irq::IRQResult::Processed;
    }

    Result AHCIDevice::ResetPort(Port& p)
    {
        int n = p.p_num;

        /* Force a hard port reset */
        Write(AHCI_REG_PxSCTL(n), AHCI_PxSCTL_DET(AHCI_DET_RESET));
        delay(100);
        Write(AHCI_REG_PxSCTL(n), Read(AHCI_REG_PxSCTL(n)) & ~AHCI_PxSCTL_DET(AHCI_DET_RESET));
        delay(100);

        /* Wait until the port shows some life */
        int port_delay = 100;
        while (port_delay > 0) {
            int detd = AHCI_PxSSTS_DETD(Read(AHCI_REG_PxSSTS(n)));
            if (detd == AHCI_DETD_DEV_NO_PHY || detd == AHCI_DETD_DEV_PHY)
                break;
            delay(1);
            port_delay--;
        }
        if (port_delay == 0) {
            Printf("port #%d not responding; no device?", n);
            return RESULT_MAKE_FAILURE(ENODEV);
        }

        /*
         * Okay, we now need to clear the error register; this should allow us to
         * transition to active mode
         */
        Write(AHCI_REG_PxSERR(n), 0xffffffff);

        port_delay = 1000;
        while (port_delay > 0) {
            int detd = AHCI_PxSSTS_DETD(Read(AHCI_REG_PxSSTS(n)));
            if (detd == AHCI_DETD_DEV_PHY)
                break;
            delay(1);
            port_delay--;
        }
        if (port_delay == 0) {
            Printf("port #%d doesn't go to dev/phy", n);
            return RESULT_MAKE_FAILURE(ENODEV);
        }

        /* Now wait for a bit until the port's BSY flag drops */
        port_delay = 1000;
        while (port_delay > 0) {
            uint32_t tfd = Read(AHCI_REG_PxTFD(n));
            if ((tfd & 0x80) == 0)
                break;
            delay(10);
            port_delay--;
        }

        if (port_delay == 0)
            Printf("port #%d alive but never clears bsy?!", n);
        else
            Printf("port #%d okay", n);

        /* Reset any port errors */
        Write(AHCI_REG_PxSERR(n), 0xffffffff);

        /* Clear any pending interrupts */
        Write(AHCI_REG_PxIS(n), 0xffffffff);

        return Result::Success();
    }

    Result AHCIDevice::Attach()
    {
        char* res_mem =
            static_cast<char*>(d_ResourceSet.AllocateResource(Resource::RT_Memory, 4096));
        if (res_mem == nullptr)
            return RESULT_MAKE_FAILURE(ENODEV);

        /* Enable busmastering; all communication is done by DMA */
        pci_enable_busmaster(*this, 1);

        /* XXX I wonder if the BIOS/OS handoff (10.6) should happen here? */
        if (*(volatile uint32_t*)(res_mem + AHCI_REG_CAP2) & AHCI_CAP2_BOH) {
            Printf("NEED BIOS stuff");
        }

        /* Enable AHCI aware mode, but disable interrupts */
        *(volatile uint32_t*)(res_mem + AHCI_REG_GHC) = AHCI_GHC_AE;

        /* Reset the controller */
        *(volatile uint32_t*)(res_mem + AHCI_REG_GHC) |= AHCI_GHC_HR;
        int reset_wait = 1000;
        while (reset_wait > 0) {
            if ((*(volatile uint32_t*)(res_mem + AHCI_REG_GHC) & AHCI_GHC_HR) == 0)
                break;
            reset_wait--;
            delay(1);
        }
        if (reset_wait == 0) {
            Printf("controller never leaves reset, giving up");
            return RESULT_MAKE_FAILURE(ENODEV);
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

        /* Initialize the per-port structures */
        ap_port = new Port*[num_ports];
        for (unsigned int n = 0; n < num_ports; n++)
            ap_port[n] = nullptr;
        ap_addr = (addr_t)res_mem;
        ap_pi = pi;
        ap_num_ports = num_ports;

        /* Create DMA tags; we need those to do DMA */
        {
            using namespace dma::limits;
            d_DMA_tag = dma::CreateTag(
                d_Parent->d_DMA_tag, anyAlignment, minAddr, max32BitAddr, 1, maxSegmentSize);
        }

        uint32_t cap = Read(AHCI_REG_CAP);
        ap_ncs = AHCI_CAP_NCS(cap) + 1;

        if (auto result = GetBusDeviceOperations().AllocateIRQ(*this, 0, *this); result.IsFailure())
            return result;

        /* Force all ports into idle mode */
        int need_wait = 0;
        for (unsigned int n = 0; n < 32; n++) {
            if ((ap_pi & AHCI_PI_PI(n)) == 0)
                continue;

            /* Check if the port is idle; if this is true, we needn't do anything here */
            if ((Read(AHCI_REG_PxCMD(n)) &
                 (AHCI_PxCMD_ST | AHCI_PxCMD_CR | AHCI_PxCMD_FRE | AHCI_PxCMD_FR)) == 0)
                continue;

            /* Port still running; reset ST and FRE (this is acknowledged by the CR and FR being
             * cleared) */
            Write(AHCI_REG_PxCMD(n), (Read(AHCI_REG_PxCMD(n)) & ~(AHCI_PxCMD_ST | AHCI_PxCMD_FRE)));
            need_wait++;
        }

        /* If we had to reset ports, wait until they are all okay */
        if (need_wait) {
            delay(500);
            int ok = 1;

            /* Check if they are all okay */
            for (unsigned int n = 0; n < 32; n++) {
                if ((ap_pi & AHCI_PI_PI(n)) == 0)
                    continue;

                if ((Read(AHCI_REG_PxCMD(n)) & (AHCI_PxCMD_CR | AHCI_PxCMD_FR)) != 0)
                    ok = 0;
            }
            if (!ok) {
                /* XXX we can and should recover from this */
                Printf("not all ports reset correctly");
                return RESULT_MAKE_FAILURE(ENODEV);
            }
        }

        /* Allocate memory and program buffers for all usable ports */
        int idx = 0;
        for (unsigned int n = 0; n < 32; n++) {
            if ((ap_pi & AHCI_PI_PI(n)) == 0)
                continue;

            // Make the device here; we'll initialize part of it shortly. Do not call
            // attachSingle() as we need interrupts for that, which we can't handle yet
            ResourceSet resourceSet;
            resourceSet.AddResource(Resource(Resource::RT_ChildNum, n, 0));
            Port* p = static_cast<Port*>(device_manager::CreateDevice(
                "ahci-port", CreateDeviceProperties(*this, resourceSet)));
            KASSERT(p != nullptr, "unable to create port?");
            ap_port[idx] = p;
            idx++;

            /* Create DMA-able memory buffers for the command list and RFIS */
            if (auto result = d_DMA_tag->AllocateBuffer(1024, p->p_dmabuf_cl); result.IsFailure())
                return result;
            if (auto result = d_DMA_tag->AllocateBuffer(256, p->p_dmabuf_rfis); result.IsFailure())
                return result;

            p->p_cle =
                static_cast<struct AHCI_PCI_CLE*>(p->p_dmabuf_cl->GetSegments().front().s_virt);
            p->p_rfis =
                static_cast<struct AHCI_PCI_RFIS*>(p->p_dmabuf_rfis->GetSegments().front().s_virt);
            p->p_num = n;

            /* Program our command list and FIS buffer addresses */
            uint64_t addr_cl = p->p_dmabuf_cl->GetSegments().front().s_phys;
            Write(AHCI_REG_PxCLB(n), addr_cl & 0xffffffff);
            Write(AHCI_REG_PxCLBU(n), addr_cl >> 32);
            uint64_t addr_rfis = p->p_dmabuf_rfis->GetSegments().front().s_phys;
            Write(AHCI_REG_PxFB(n), addr_rfis & 0xffffffff);
            Write(AHCI_REG_PxFBU(n), addr_rfis >> 32);
            AHCI_DPRINTF("## port %d command list at %p rfis at %p", n, addr_cl, addr_rfis);

            /* Activate the channel */
            Write(
                AHCI_REG_PxCMD(n),
                AHCI_PxCMD_ICC(AHCI_ICC_ACTIVE) | AHCI_PxCMD_POD | AHCI_PxCMD_SUD);

            /*
             * Enable receiving of FISes; we'll remain BSY without this as we need to
             * retrieve the device's signature...
             */
            Write(AHCI_REG_PxCMD(n), Read(AHCI_REG_PxCMD(n)) | AHCI_PxCMD_FRE);

            /* Reset the port to ensure the device is in a sane state */
            ResetPort(*p);

            /* XXX For now, be extra interrupt-prone */
            Write(
                AHCI_REG_PxIE(n),
                AHCI_PxIE_CPDE | AHCI_PxIE_TFEE | AHCI_PxIE_HBFE | AHCI_PxIE_HBDE | AHCI_PxIE_IFE |
                    AHCI_PxIE_INFE | AHCI_PxIE_OFE | AHCI_PxIE_IPME | AHCI_PxIE_PRCE |
                    AHCI_PxIE_DMPE | AHCI_PxIE_PCE | AHCI_PxIE_DPE | AHCI_PxIE_UFE |
                    AHCI_PxIE_SDBE | AHCI_PxIE_DSE | AHCI_PxIE_PSE | AHCI_PxIE_DHRE);
        }

        /* Enable global AHCI interrupts */
        Write(AHCI_REG_IS, 0xffffffff);
        Write(AHCI_REG_GHC, Read(AHCI_REG_GHC) | AHCI_GHC_IE);

        /*
         * Interrupts are enabled; ports should be happy - now, attach all devices on
         * the ports.
         */
        idx = 0;
        for (unsigned int n = 0; n < 32; n++) {
            if ((ap_pi & AHCI_PI_PI(n)) == 0)
                continue;
            Port* p = ap_port[idx++];

            // Attach the port; things like disks will be attached on top of it
            device_manager::AttachSingle(*p); /* XXX check error */
        }

        return Result::Success();
    }

    Result AHCIDevice::Detach()
    {
        panic("TODO");
        return Result::Success();
    }

} // namespace ahci

namespace
{
    struct AHCI_PCI_Driver : public Driver {
        AHCI_PCI_Driver() : Driver("ahcipci") {}

        const char* GetBussesToProbeOn() const override { return "pcibus"; }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PCI_ClassRev, 0);
            if (res == NULL)
                return nullptr;
            uint32_t classrev = res->r_Base;

            /* Anything AHCI will do */
            if (PCI_CLASS(classrev) == PCI_CLASS_STORAGE &&
                PCI_SUBCLASS(classrev) == PCI_SUBCLASS_SATA &&
                PCI_REVISION(classrev) == 1 /* AHCI */)
                return new ahci::AHCIDevice(cdp);

            /* And some specific devices which pre-date this schema */
            res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PCI_VendorID, 0);
            uint32_t vendor = res->r_Base;
            res = cdp.cdp_ResourceSet.GetResource(Resource::RT_PCI_DeviceID, 0);
            uint32_t device = res->r_Base;
            if (vendor == 0x8086 && device == 0x2922) /* Intel ICH9, like what is in QEMU */
                return new ahci::AHCIDevice(cdp);
            if (vendor == 0x8086 && device == 0x2829) /* Intel ICH8M, like what is in VirtualBox */
                return new ahci::AHCIDevice(cdp);
            if (vendor == 0x10de && device == 0x7f4) /* NForce 630i SATA */
                return new ahci::AHCIDevice(cdp);
            if (vendor == 0x1039 && device == 0x1185) /* SiS AHCI Controller (0106) */
                return new ahci::AHCIDevice(cdp);
            if (vendor == 0x1022 && device == 0x7801) /* AMD FCH SATA Controller */
                return new ahci::AHCIDevice(cdp);
            return nullptr;
        }
    };

    const RegisterDriver<AHCI_PCI_Driver> registerDriver;

} // unnamed namespace

/* vim:set ts=2 sw=2: */
