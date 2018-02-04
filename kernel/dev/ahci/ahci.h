#ifndef __ANANAS_AHCI2_H__
#define __ANANAS_AHCI2_H__

#include "kernel/dev/sata.h"
#include "kernel/dma.h"
#include "kernel/irq.h"

#define AHCI_DEBUG 0

#if AHCI_DEBUG
#define AHCI_DPRINTF(fmt, ...) \
	device_printf(dev, fmt, __VA_ARGS__)
#else
#define AHCI_DPRINTF(...) (void)0
#endif

namespace Ananas {
namespace AHCI {

struct AHCI_PCI_CT;

class AHCIDevice;

struct Request {
	struct SATA_REQUEST	pr_request;
	dma_buf_t pr_dmabuf_ct;
	struct AHCI_PCI_CT*	pr_ct;
};

class Port final : public Ananas::Device, private Ananas::IDeviceOperations {
public:
	Port(const Ananas::CreateDeviceProperties& cdp);
	virtual ~Port() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	Result Attach() override;
	Result Detach() override;

	void Enqueue(void* request);
	void Start();

	AHCIDevice& p_device;		/* [RO] Device we belong to */
	int p_num;			/* [RO] Port number */
	dma_buf_t p_dmabuf_cl;		/* [RO] DMA buffer for command list */
	dma_buf_t p_dmabuf_rfis;	/* [RO] DMA buffer for RFIS */

	struct AHCI_PCI_CLE* p_cle;
	struct AHCI_PCI_RFIS* p_rfis;
	uint32_t p_request_in_use;	/* [RW] Current requests in use */
	uint32_t p_request_valid;	/* [RW] Requests that can be activated */
	uint32_t p_request_active;	/* [RW] Requests that are activated */
	struct Request p_request[32];

	void OnIRQ(uint32_t pis);

private:
	void Lock() {
		p_lock.Lock();
	}

	void Unlock() {
		p_lock.Unlock();
	}

	Spinlock p_lock;
};

class AHCIDevice : public Ananas::Device, private Ananas::IDeviceOperations
{
	friend class Port;
public:
	using Device::Device;
	virtual ~AHCIDevice() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	Result Attach() override;
	Result Detach() override;

protected:
	inline void Write(unsigned int reg, uint32_t val)
	{
		*(volatile uint32_t*)(ap_addr + reg) = val;
	}

	inline uint32_t Read(unsigned int reg)
	{
		return *(volatile uint32_t*)(ap_addr + reg);
	}

	void OnIRQ();

	static IRQResult IRQWrapper(Ananas::Device* device, void* context)
	{
		auto ahci = static_cast<AHCIDevice*>(device);
		ahci->OnIRQ();
		return IRQResult::IR_Processed;
	}


	Result ResetPort(Port& p);


private:
	// Device address
	addr_t ap_addr;
	uint32_t ap_pi;
	unsigned int ap_ncs;
	unsigned int ap_num_ports;
	Port** ap_port;
};

} // namespace AHCI
} // namespace Ananas

#endif // __ANANAS_AHCI2_H__
