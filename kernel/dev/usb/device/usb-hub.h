#ifndef __ANANAS_USB_HUB_H__
#define __ANANAS_USB_HUB_H__

namespace usb {

struct HUB_PORT_STATUS {
	uint16_t	ps_portstatus;
#define HUB_PORTSTATUS_CONNECTED	(1 << 0)		/* Device is present on this port */
#define HUB_PORTSTATUS_ENABLED		(1 << 1)		/* Port is enabled */
#define HUB_PORTSTATUS_SUSPENDED	(1 << 2)		/* Port is suspended or resuming */
#define HUB_PORTSTATUS_OVERCURRENT	(1 << 3)		/* Over-current condition present */
#define HUB_PORTSTATUS_RESET		(1 << 4)		/* Port is resetting */
#define HUB_PORTSTATUS_POWERED		(1 << 8)		/* Port is powered */
#define HUB_PORTSTATUS_LOWSPEED		(1 << 9)		/* Low speed device attached */
	uint16_t	ps_portchange;
#define HUB_PORTCHANGE_CONNECT		(1 << 0)		/* Connection state has changed */
#define HUB_PORTCHANGE_ENABLE		(1 << 1)		/* Enable state has changed */
#define HUB_PORTCHANGE_SUSPEND		(1 << 2)		/* Resume complete */
#define HUB_PORTCHANGE_OVERCURRENT	(1 << 3)		/* Over-current has changed */
#define HUB_PORTCHANGE_RESET		(1 << 4)		/* Reset complete */
};

class Hub : public Device, private IDeviceOperations, private IUSBHubDeviceOperations, private IPipeCallback
{
	class Port;
public:
	using Device::Device;
	virtual ~Hub();

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	IUSBHubDeviceOperations* GetUSBHubDeviceOperations() override
	{
		return this;
	}

	Result Attach() override;
	Result Detach() override;
	Result ResetPort(int n);
	void HandleExplore() override;

protected:
	void OnPipeCallback(Pipe& pipe) override;

	void ExploreNewDevice(Port& port, int n);
	void HandleDetach(Port& port, int n);

private:
	Pipe* h_Pipe = nullptr;
	class Port
	{
	public:
		int	p_flags = 0;
#define HUB_PORT_FLAG_CONNECTED		(1 << 0)		/* Device is connected */
#define HUB_PORT_FLAG_UPDATED		(1 << 1)		/* Port status is updated */
		USBDevice* p_device = nullptr;
	};

	int		hub_flags = 0;
#define HUB_FLAG_SELFPOWERED		(1 << 0)		/* Hub is self powered */
#define HUB_FLAG_UPDATED		(1 << 1)		/* Hub status needs updating */
	USBDevice* h_Device = nullptr;
	int	h_NumPorts = 0;
	Port**	h_Port = nullptr;
};

} // namespace usb

#endif /* __ANANAS_USB_HUB_H__ */
