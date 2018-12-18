/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef __ANANAS_AHCI2_H__
#define __ANANAS_AHCI2_H__

#include "kernel/dev/sata.h"
#include "kernel/irq.h"

#define AHCI_DEBUG 0

#if AHCI_DEBUG
#define AHCI_DPRINTF(fmt, ...) device_printf(dev, fmt, __VA_ARGS__)
#else
#define AHCI_DPRINTF(...) (void)0
#endif

namespace dma
{
    class Buffer;
}

namespace ahci
{
    struct AHCI_PCI_CT;

    class AHCIDevice;

    struct Request {
        struct SATA_REQUEST pr_request;
        dma::Buffer* pr_dmabuf_ct = nullptr;
        struct AHCI_PCI_CT* pr_ct = nullptr;
    };

    class Port final : public Device, private IDeviceOperations
    {
      public:
        Port(const CreateDeviceProperties& cdp);
        virtual ~Port() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

        void Enqueue(void* request);
        void Start();

        AHCIDevice& p_device;                 /* [RO] Device we belong to */
        int p_num;                            /* [RO] Port number */
        dma::Buffer* p_dmabuf_cl = nullptr;   /* [RO] DMA buffer for command list */
        dma::Buffer* p_dmabuf_rfis = nullptr; /* [RO] DMA buffer for RFIS */

        struct AHCI_PCI_CLE* p_cle;
        struct AHCI_PCI_RFIS* p_rfis;
        uint32_t p_request_in_use; /* [RW] Current requests in use */
        uint32_t p_request_valid;  /* [RW] Requests that can be activated */
        uint32_t p_request_active; /* [RW] Requests that are activated */
        struct Request p_request[32];

        void OnIRQ(uint32_t pis);

      private:
        void Lock() { p_lock.Lock(); }

        void Unlock() { p_lock.Unlock(); }

        Spinlock p_lock;
    };

    class AHCIDevice : public Device, private IDeviceOperations, private irq::IHandler
    {
        friend class Port;

      public:
        using Device::Device;
        virtual ~AHCIDevice() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        Result Attach() override;
        Result Detach() override;

      protected:
        inline void Write(unsigned int reg, uint32_t val)
        {
            *(volatile uint32_t*)(ap_addr + reg) = val;
        }

        inline uint32_t Read(unsigned int reg) { return *(volatile uint32_t*)(ap_addr + reg); }

        irq::IRQResult OnIRQ() override;

        Result ResetPort(Port& p);

      private:
        // Device address
        addr_t ap_addr;
        uint32_t ap_pi;
        unsigned int ap_ncs;
        unsigned int ap_num_ports;
        Port** ap_port;
    };

} // namespace ahci

#endif // __ANANAS_AHCI2_H__
