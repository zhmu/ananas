#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/time.h"
#include "kernel/trace.h"
#include "../core/config.h"
#include "../core/usb-bus.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"
#include "usb-hub.h"

namespace Ananas {
namespace USB {

#if 0
# define DPRINTF Printf
#else
# define DPRINTF(...)
#endif

TRACE_SETUP;

Hub::~Hub()
{
	for (int n = 0; n < h_NumPorts; n++)
		delete h_Port[n];
	delete[] h_Port;
}

void
Hub::OnPipeCallback(Pipe& pipe)
{
	Transfer& xfer = pipe.p_xfer;

	/* Error means nothing happened, so request a new transfer */
	if (xfer.t_flags & TRANSFER_FLAG_ERROR) {
		Printf("usbhub_int_callback: error!");
		return;
	}

	if (xfer.t_data[0] & 1) {
		/* Hub status changed - need to fetch the new status */
		hub_flags |= HUB_FLAG_UPDATED;
	}

	int num_changed = 0;
	for (int n = 1; n <= h_NumPorts; n++) {
		if ((xfer.t_data[n / 8] & (1 << (n & 7))) == 0)
			continue;

		/* This port was updated - fetch the port status */
		h_Port[n - 1]->p_flags |= HUB_PORT_FLAG_UPDATED;
		num_changed++;
	}

	/* If anything was changed, we need to schedule a bus explore */
	if (num_changed > 0)
		h_Device->ud_bus.ScheduleExplore();

	/* Reschedule the pipe for future updates */
	h_Pipe->Start();
}

Result
Hub::ResetPort(int n)
{
	KASSERT(n >= 1 && n <= h_NumPorts, "port %d out of range", n);

	/* Reset the reset state of the port in case it lingers */
	DPRINTF("%s: port %d: clearing c_port_reset", __func__, n);
	if (auto result = h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_CLEAR_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_C_PORT_RESET, n, NULL, NULL, true); result.IsFailure()) {
		Printf("port_clear error %d, continuing anyway", result.AsStatusCode());
	}

	/* Need to reset the port */
	DPRINTF("%s: port %d: resetting", __func__, n);
	if (auto result = h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_SET_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_PORT_RESET, n, NULL, NULL, true); result.IsFailure()) {
		Printf("port_reset error %d, ignoring port", result.AsStatusCode());
		return result;
	}

	struct HUB_PORT_STATUS ps;
	int timeout = 10; /* XXX */
	while(timeout > 0) {
		delay(100);

		/* See if the device is correctly reset */
		size_t len = sizeof(ps);
		memset(&ps, 0, len);
		if (auto result = h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_GET_STATUS, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, 0, n, &ps, &len, false); result.IsFailure()) {
			Printf("get_status error %d, ignoring port", result.AsStatusCode());
			return result;
		}

		if ((ps.ps_portstatus & USB_HUB_PS_PORT_CONNECTION) == 0) {
			/* Port is no longer attached; give up */
			Printf("port %d no longer connected, ignoring port", n);
			return Result::Success();
		}

		if (ps.ps_portchange & HUB_PORTCHANGE_RESET)
			break;

		timeout--;
	}
	if (timeout == 0) {
		Printf("timeout resetting port %d", n);
		return RESULT_MAKE_FAILURE(ENODEV);
	}

	DPRINTF("%s: port %d: reset completed; clearing c_reset", __func__, n);
	if (auto result = h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_CLEAR_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_C_PORT_RESET, n, NULL, NULL, true); result.IsFailure()) {
		Printf("unable to clear reset of port %d", n);
		return result;
	}

	return Result::Success();
}

void
Hub::ExploreNewDevice(Port& port, int n)
{
	KASSERT(port.p_device == nullptr, "exploring new device over current?");

	/* Fetch the port status; we need to know if it's lo- or high speed */
	struct HUB_PORT_STATUS ps;
	size_t len = sizeof(ps);
	memset(&ps, 0, len);
	if (auto result = h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_GET_STATUS, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, 0, n, &ps, &len, false); result.IsFailure()) {
		Printf("get_status(%d) error %d, ignoring port", n, result.AsStatusCode());
		return;
	}

	/* If the port is no longer connected, we needn't do anything */
	if ((ps.ps_portstatus & USB_HUB_PS_PORT_CONNECTION) == 0) {
		Printf("port %d no longer connected, giving up", n);
		return;
	}

	/* Mark the port as attached */
	port.p_flags |= HUB_PORT_FLAG_CONNECTED;

	/* Hand it off to the USB framework */
	int flags = (ps.ps_portstatus & USB_HUB_PS_PORT_LOW_SPEED) != 0;
	port.p_device = new USBDevice(h_Device->ud_bus, this, n, flags);
	ScheduleAttach(*port.p_device);
}

void
Hub::HandleDetach(Port& port, int n)
{
	USBDevice* usb_dev = port.p_device;
	KASSERT(usb_dev != nullptr, "detaching null device");

	if (auto result = usb_dev->Detach(); result.IsFailure()) {
		Printf("unable to detach device (%d)", result.AsStatusCode());
		return;
	}
	// usb_dev is no longer valid at this point

	/* Deregister the device and mark the port as disconnected */
	port.p_device = nullptr;
	port.p_flags &= ~HUB_PORT_FLAG_CONNECTED;
}

void
Hub::HandleExplore()
{
	if (hub_flags & HUB_FLAG_UPDATED) {
		Printf("hub updated, todo");
		hub_flags &= ~HUB_FLAG_UPDATED;
	}

	/* Handle all ports that need handling */
	for (int n = 1; n <= h_NumPorts; n++) {
		Port& port = *h_Port[n - 1];
		if ((port.p_flags & HUB_PORT_FLAG_UPDATED) == 0)
			continue;

		struct HUB_PORT_STATUS ps;
		size_t len = sizeof(ps);
		if (auto result = h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_GET_STATUS, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, 0, n, &ps, &len, false); result.IsFailure()) {
			Printf("get_status error %d, ignoring port", result.AsStatusCode());
			continue;
		}

		if (ps.ps_portchange & HUB_PORTCHANGE_ENABLE) {
			DPRINTF("%s: port %d: enabled changed, clearing c_port_enable", __func__, n);
			h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_CLEAR_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_C_PORT_ENABLE, n, NULL, NULL, true);
		}

		if (ps.ps_portchange & HUB_PORTCHANGE_CONNECT) {
			/* Port connection changed - acknowledge this */
			DPRINTF("%s: port %d: connect changed, clearing c_port_connnection", __func__, n);
			h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_CLEAR_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_C_PORT_CONNECTION, n, NULL, NULL, true);

			if ((port.p_flags & HUB_PORT_FLAG_CONNECTED) == 0) {
				/* Nothing was connected to there; need to hook something up */
				ExploreNewDevice(port, n);
			} else {
				HandleDetach(port, n);
			}
		}
	}
}

Result
Hub::Attach()
{
	h_Device = static_cast<USBDevice*>(d_ResourceSet.AllocateResource(Ananas::Resource::RT_USB_Device, 0));

	/* Obtain the hub descriptor */
	struct USB_DESCR_HUB hd;
	{
		size_t len = sizeof(hd);
		RESULT_PROPAGATE_FAILURE(
			h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_GET_DESC, USB_CONTROL_RECIPIENT_DEVICE, USB_CONTROL_TYPE_CLASS, USB_REQUEST_MAKE(USB_DESCR_TYPE_HUB, 0), 0, &hd, &len, false)
		);
	}

	h_NumPorts = hd.hd_numports;
	h_Port = new Port*[h_NumPorts];

	for(int n = 0; n < h_NumPorts; n++)
		h_Port[n] = new Port;
	Printf("%d port(s)", h_NumPorts);

	/* Enable power to all ports */
	Result result = Result::Success();
	for (int n = 0; n < h_NumPorts; n++) {
		result = h_Device->PerformControlTransfer(USB_CONTROL_REQUEST_SET_FEATURE, USB_CONTROL_RECIPIENT_OTHER, USB_CONTROL_TYPE_CLASS, HUB_FEATURE_PORT_POWER, n + 1, NULL, NULL, true);
		if (result.IsFailure())
			goto fail;

		/* Force the port as 'updated' - we need to check it initially */
		h_Port[n]->p_flags = HUB_PORT_FLAG_UPDATED;

		/* Wait until the power is good */
		delay(hd.hd_poweron2good * 2 + 10 /* slack */);
	}

	/* Initialization went well; hook up the interrupt pipe so that we may receive updates */
	result = h_Device->AllocatePipe(0, TRANSFER_TYPE_INTERRUPT, EP_DIR_IN, 0, *this, h_Pipe);
	if (result.IsFailure()) {
		Printf("endpoint 0 not interrupt/in");
		goto fail;
	}
	return h_Pipe->Start();
fail:
	return result;
}

Result
Hub::Detach()
{
	if (h_Pipe != nullptr) {
		h_Device->FreePipe(*h_Pipe);
		h_Pipe = nullptr;
	}

	h_Device->ud_bus.DetachHub(*this);
	return Result::Success();
}

namespace {

struct USBHub_Driver : public Ananas::Driver
{
	USBHub_Driver()
	 : Driver("usbhub")
	{
	}

	const char* GetBussesToProbeOn() const override
	{
		return "usbbus";
	}

	Ananas::Device* CreateDevice(const Ananas::CreateDeviceProperties& cdp) override
	{
		auto res = cdp.cdp_ResourceSet.GetResource(Ananas::Resource::RT_USB_Device, 0);
		if (res == nullptr)
			return nullptr;
		auto usb_dev = static_cast<USBDevice*>(reinterpret_cast<void*>(res->r_Base));

		Interface& iface = usb_dev->ud_interface[usb_dev->ud_cur_interface];
		if (iface.if_class == USB_IF_CLASS_HUB)
			return new Hub(cdp);
		return nullptr;
	}
};

} // unnamed namespace

REGISTER_DRIVER(USBHub_Driver)

} // namespace USB
} // namespace Ananas

/* vim:set ts=2 sw=2: */
