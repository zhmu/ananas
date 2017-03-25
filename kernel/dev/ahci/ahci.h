#ifndef __ANANAS_AHCI2_H__
#define __ANANAS_AHCI2_H__

#include <ananas/dev/sata.h>
#include <ananas/irq.h>
#include <ananas/dma.h>
#include <ananas/dev/ahci-pci.h>

namespace Ananas {
namespace AHCI {

class AHCIDevice;

struct Request {
	struct SATA_REQUEST	pr_request;
	dma_buf_t pr_dmabuf_ct;
	struct AHCI_PCI_CT*	pr_ct;
};

class Port : public Ananas::Device, private Ananas::IDeviceOperations {
public:
	Port(const Ananas::CreateDeviceProperties& cdp);
	virtual ~Port() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	errorcode_t Attach() override;
	errorcode_t Detach() override;

	void Enqueue(void* request);
	void Start();

	spinlock_t p_lock;
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

	errorcode_t Attach() override;
	errorcode_t Detach() override;

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

	static irqresult_t IRQWrapper(Ananas::Device* device, void* context)
	{
		auto ahci = static_cast<AHCIDevice*>(device);
		ahci->OnIRQ();
		return IRQ_RESULT_PROCESSED;
	}


	errorcode_t ResetPort(Port& p);


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
