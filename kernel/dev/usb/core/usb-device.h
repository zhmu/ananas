#ifndef __ANANAS_USB_DEVICE_H__
#define __ANANAS_USB_DEVICE_H__

#include "kernel/lock.h"
#include "usb-core.h"
#include "usb-transfer.h"

class Device;

namespace usb
{
    class Bus;
    class Hub;
    class Pipe;
    class Transfer;
    class USBDevice;

    class IPipeCallback
    {
      public:
        virtual void OnPipeCallback(Pipe&) = 0;
    };

    class Pipe : public util::List<Pipe>::NodePtr
    {
      public:
        Pipe(USBDevice& usb_dev, Endpoint& ep, Transfer& xfer, IPipeCallback& callback)
            : p_dev(usb_dev), p_ep(ep), p_xfer(xfer), p_callback(callback)
        {
        }

        Pipe(const Pipe&) = delete;
        Pipe& operator=(const Pipe&) = delete;

        Result Start();
        Result Stop();

        USBDevice& p_dev;
        Endpoint& p_ep;
        Transfer& p_xfer;
        IPipeCallback& p_callback;
    };

    typedef util::List<Pipe> PipeList;

    /*
     * An USB device consists of a name, an address, pipes and a driver. We
     * augment the existing device for this and consider the USB_DEVICE as the
     * private data for the device in question.
     *
     * Fields marked with [S] are static and won't change after initialization; fields
     * with [M] use the mutex to protect them.
     */
    class USBDevice : public util::List<USBDevice>::NodePtr
    {
      public:
        USBDevice(Bus& bus, Hub* hub, int hub_port, int flags);
        ~USBDevice();

        USBDevice(const USBDevice&) = delete;
        USBDevice& operator=(const USBDevice&) = delete;

        static const size_t s_DefaultMaxPacketSz0 = 8;

        Bus& ud_bus; /* [S] Bus the device resides on */
        Hub* ud_hub; /* [S] Hub the device is attached to, or nullptr for the root hub */
        int ud_port; /* [S] Port of the hub the device is attached to */
        Device* ud_device = nullptr;                   /* [S] Device reference */
        int ud_flags;                                  /* [S] Device flags */
        int ud_address = 0;                            /* Assigned USB address */
        int ud_max_packet_sz0 = s_DefaultMaxPacketSz0; /* Maximum packet size for endpoint 0 */
        Interface ud_interface[USB_MAX_INTERFACES];
        int ud_num_interfaces = 0;
        int ud_cur_interface = -1;
        struct USB_DESCR_DEVICE ud_descr_device;

        /* Pending transfers for this device */
        PendingTransferList ud_transfers; /* [M] */

        Result Attach();
        Result Detach(); // called with bus lock held

        Result AllocatePipe(
            int num, int type, int dir, size_t maxlen, IPipeCallback& callback, Pipe*& pipe);
        void FreePipe(Pipe& pipe);

        void Lock() { ud_mutex.Lock(); }

        void Unlock() { ud_mutex.Unlock(); }

        void AssertLocked() { ud_mutex.AssertLocked(); }

        Result PerformControlTransfer(
            int req, int recipient, int type, int value, int index, void* buf, size_t* len,
            bool write);

      private:
        void FreePipe_Locked(Pipe& pipe);

        Mutex ud_mutex{"usbdev"};
        PipeList ud_pipes; /* [M] */
    };

#define USB_DEVICE_FLAG_LOW_SPEED (1 << 0)
#define USB_DEVICE_FLAG_ROOT_HUB (1 << 31)

} // namespace usb

#endif /* __ANANAS_USB_DEVICE_H__ */
