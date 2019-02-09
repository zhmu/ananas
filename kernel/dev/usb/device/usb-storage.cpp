/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include <ananas/types.h>
#include "kernel/device.h"
#include "kernel/driver.h"
#include "kernel/endian.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/trace.h"
#include "../core/usb-core.h"
#include "../core/usb-device.h"
#include "../core/usb-transfer.h"
#include "../core/config.h"
#include "../../scsi/scsi.h" // XXX kludge

TRACE_SETUP;

#if 0
#define DPRINTF Printf
#else
#define DPRINTF(...)
#endif

namespace
{
    struct USBSTORAGE_CBW {
        /* 00-03 */ uint32_t d_cbw_signature;
#define USBSTORAGE_CBW_SIGNATURE 0x43425355
        /* 04-07 */ uint32_t d_cbw_tag;
        /* 08-0b */ uint32_t d_cbw_data_transferlength;
        /* 0c */ uint8_t d_bm_cbwflags;
#define USBSTORAGE_CBW_FLAG_DATA_OUT (0 << 7)
#define USBSTORAGE_CBW_FLAG_DATA_IN (1 << 7)
        /* 0d */ uint8_t d_cbw_lun;
        /* 0e */ uint8_t d_cbw_cblength;
        /* 0f-1e */ uint8_t d_cbw_cb[16];
    } __attribute__((packed));

    struct USBSTORAGE_CSW {
        /* 00-03 */ uint32_t d_csw_signature;
#define USBSTORAGE_CSW_SIGNATURE 0x53425355
        /* 04-07 */ uint32_t d_csw_tag;
        /* 08-0b */ uint32_t d_csw_data_residue;
        /* 0c */ uint8_t d_csw_status;
#define USBSTORAGE_CSW_STATUS_GOOD 0x00
#define USBSTORAGE_CSW_STATUS_FAIL 0x01
#define USBSTORAGE_CSW_STATUS_PHASE_ERROR 0x02
    } __attribute__((packed));

#if 0
void
DumpCBW(USBSTORAGE_CBW& cbw)
{
	kprintf("signature %x tag %x data_transferlen %x\n",
	 cbw.d_cbw_signature, cbw.d_cbw_tag, cbw.d_cbw_data_transferlength);
	kprintf("cbwflags %x lun %d cblength %d cb",
	 cbw.d_bm_cbwflags, cbw.d_cbw_lun, cbw.d_cbw_cblength);
	for(unsigned int n = 0; n < 16; n++)
		kprintf(" %x", cbw.d_cbw_cb[n]);
	kprintf("\n");
}
#endif

    class USBStorage;

    struct StorageDevice_PipeInCallbackWrapper : public usb::IPipeCallback {
        StorageDevice_PipeInCallbackWrapper(USBStorage& device) : pi_Device(device) {}

        void OnPipeCallback(usb::Pipe& pipe) override;

        USBStorage& pi_Device;
    };

    struct StorageDevice_PipeOutCallbackWrapper : public usb::IPipeCallback {
        StorageDevice_PipeOutCallbackWrapper(USBStorage& device) : pi_Device(device) {}

        void OnPipeCallback(usb::Pipe& pipe) override;

        USBStorage& pi_Device;
    };

    class USBStorage : public Device, private IDeviceOperations, private ISCSIDeviceOperations
    {
      public:
        USBStorage(const CreateDeviceProperties& cdp);
        virtual ~USBStorage() = default;

        IDeviceOperations& GetDeviceOperations() override { return *this; }

        ISCSIDeviceOperations* GetSCSIDeviceOperations() override { return this; }

        void OnPipeInCallback();
        void OnPipeOutCallback();

        Result PerformSCSIRequest(
            int lun, ISCSIDeviceOperations::Direction dir, const void* cb, size_t cb_len,
            void* result, size_t* result_len) override;

      protected:
        Result Attach() override;
        Result Detach() override;

      private:
        void Lock() { us_mutex.Lock(); }

        void Unlock() { us_mutex.Unlock(); }

        usb::USBDevice* us_Device = nullptr;
        usb::Pipe* us_BulkIn = nullptr;
        usb::Pipe* us_BulkOut = nullptr;

        StorageDevice_PipeInCallbackWrapper us_PipeInCallback;
        StorageDevice_PipeOutCallbackWrapper us_PipeOutCallback;

        Mutex us_mutex{"usbstorage"};
        unsigned int us_max_lun = 0;
        /* Output buffer */
        void* us_output_buffer = nullptr;
        size_t us_output_filled = 0;
        size_t us_output_len = 0;
        /* Most recent CSW received */
        Result* us_result_ptr = nullptr;
        struct USBSTORAGE_CSW* us_csw_ptr = nullptr;

        /* Signalled when the CSW is received */
        Semaphore us_signal_sem{"usbstorage-signal", 0};
    };

    void StorageDevice_PipeInCallbackWrapper::OnPipeCallback(usb::Pipe& pipe)
    {
        pi_Device.OnPipeInCallback();
    }

    void StorageDevice_PipeOutCallbackWrapper::OnPipeCallback(usb::Pipe& pipe)
    {
        pi_Device.OnPipeOutCallback();
    }

    USBStorage::USBStorage(const CreateDeviceProperties& cdp)
        : Device(cdp), us_PipeInCallback(*this), us_PipeOutCallback(*this)
    {
    }

    Result USBStorage::PerformSCSIRequest(
        int lun, Direction dir, const void* cb, size_t cb_len, void* result, size_t* result_len)
    {
        KASSERT(result_len == nullptr || result != nullptr, "result_len without result?");
        DPRINTF(
            "dir %d len %d cb_len %d result_len %d", dir, lun, cb_len,
            result_len ? *result_len : -1);

        struct USBSTORAGE_CSW csw;

        /* Create the command-block-wrapper */
        struct USBSTORAGE_CBW cbw;
        memset(&cbw, 0, sizeof cbw);
        cbw.d_cbw_signature = USBSTORAGE_CBW_SIGNATURE;
        cbw.d_cbw_data_transferlength = (result_len != NULL) ? *result_len : 0;
        cbw.d_bm_cbwflags =
            (dir == Direction::D_In) ? USBSTORAGE_CBW_FLAG_DATA_IN : USBSTORAGE_CBW_FLAG_DATA_OUT;
        cbw.d_cbw_lun = lun;
        cbw.d_cbw_cblength = cb_len;
        memcpy(&cbw.d_cbw_cb[0], cb, cb_len);

        /* All SCSI CDB's follow a standard format; fill out those fields here XXX This is not the
         * correct place */
        switch (cb_len) {
            case 6: {
                struct SCSI_CDB_6* cdb = (struct SCSI_CDB_6*)&cbw.d_cbw_cb[0];
                cdb->c_alloc_len = (result_len != nullptr) ? *result_len : 0;
                break;
            }
            case 10: {
                struct SCSI_CDB_10* cdb = (struct SCSI_CDB_10*)&cbw.d_cbw_cb[0];
                uint16_t v = (result_len != nullptr) ? *result_len : 0;
                if (cdb->c_code != SCSI_CMD_READ_10) /* XXX */
                    cdb->c_alloc_len = htobe16(v);
                break;
            }
            default:
                panic("invalid cb_len %d", cb_len);
        }

        Lock();
        /* Ensure our output is at a sensible location */
        us_output_buffer = result;
        us_output_filled = 0;
        us_output_len = (result_len != NULL) ? *result_len : 0;
        Result r = RESULT_MAKE_FAILURE(EIO);
        us_result_ptr = &r;
        us_csw_ptr = &csw;
        /* Now, submit the request */
        us_BulkOut->p_xfer.t_length = sizeof(cbw);
        memcpy(&us_BulkOut->p_xfer.t_data[0], &cbw, us_BulkOut->p_xfer.t_length);

        us_BulkOut->Start();
        Unlock();

        /* Now we wait for the signal ... */
        us_signal_sem.WaitAndDrain();
        if (r.IsFailure())
            return r;

        /* See if the CSW makes sense */
        if (csw.d_csw_signature != USBSTORAGE_CSW_SIGNATURE)
            return RESULT_MAKE_FAILURE(EIO);
        if (csw.d_csw_tag != cbw.d_cbw_tag)
            return RESULT_MAKE_FAILURE(EIO);
        if (csw.d_csw_status != USBSTORAGE_CSW_STATUS_GOOD) {
            DPRINTF("device rejected request: %d", csw.d_csw_status);
            return RESULT_MAKE_FAILURE(EIO);
        }

        return Result::Success();
    }

    /* Called when data flows from the device -> us */
    void USBStorage::OnPipeInCallback()
    {
        auto& xfer = us_BulkIn->p_xfer;

        DPRINTF("usbstorage_in_callback! -> flags %x len %d", xfer.t_flags, xfer.t_result_length);

        /*
         * We'll have one or two responses now: the first will be the resulting data,
         * and the second will be the CSW.
         */
        bool need_schedule = false;
        Lock();
        int len = xfer.t_result_length;
        if (us_output_buffer != nullptr) {
            size_t left = us_output_len - us_output_filled;
            if (len > left)
                len = left;

            memcpy((char*)us_output_buffer + us_output_filled, &xfer.t_data[0], len);
            us_output_filled += len;
            left -= len;
            if (left == 0) {
                us_output_len = len;
                us_output_buffer = nullptr;
            }
            need_schedule = true; /* as more data will arrive */
        } else if (us_csw_ptr != nullptr && us_result_ptr != nullptr) {
            if (len != sizeof(struct USBSTORAGE_CSW)) {
                Printf(
                    "invalid csw length (expected %d got %d)", sizeof(struct USBSTORAGE_CSW), len);
                *us_result_ptr = RESULT_MAKE_FAILURE(EIO);
            } else {
                memcpy(us_csw_ptr, &xfer.t_data[0], len);
                *us_result_ptr = Result::Success();
            }
            us_result_ptr = nullptr;
            us_csw_ptr = nullptr;
            us_signal_sem.Signal();
        } else {
            Printf("received %d bytes but no sink?", len);
        }
        Unlock();

        if (need_schedule)
            us_BulkIn->Start();
    }

    void USBStorage::OnPipeOutCallback()
    {
        auto& xfer = us_BulkOut->p_xfer;
        (void)xfer;

        DPRINTF("usbstorage_out_callback! -> len %d", xfer.t_result_length);

        us_BulkIn->Start();
    }

    Result USBStorage::Attach()
    {
        us_Device = static_cast<usb::USBDevice*>(
            d_ResourceSet.AllocateResource(Resource::RT_USB_Device, 0));

        /*
         * Determine the max LUN of the device - note that devices do not have to support this,
         * so we use 0 in case they do not give this information.
         */
        uint8_t max_lun;
        size_t len = sizeof(max_lun);
        auto result = us_Device->PerformControlTransfer(
            USB_CONTROL_REQUEST_GET_MAX_LUN, USB_CONTROL_RECIPIENT_INTERFACE,
            USB_CONTROL_TYPE_CLASS, USB_REQUEST_MAKE(0, 0), 0, &max_lun, &len, false);
        if (result.IsFailure() || len != sizeof(max_lun))
            max_lun = 0;
        us_max_lun = max_lun;

        /*
         * There must be a BULK/IN and BULK/OUT endpoint - however, the spec doesn't
         * say in which order they are. To cope, we'll just try both.
         */
        int outep_index = 1;
        result = us_Device->AllocatePipe(
            0, TRANSFER_TYPE_BULK, EP_DIR_IN, 0, us_PipeInCallback, us_BulkIn);
        if (result.IsFailure()) {
            result = us_Device->AllocatePipe(
                1, TRANSFER_TYPE_BULK, EP_DIR_IN, 0, us_PipeInCallback, us_BulkIn);
            outep_index = 0;
        }
        if (result.IsFailure()) {
            Printf("no bulk/in endpoint present");
            return result;
        }
        result = us_Device->AllocatePipe(
            outep_index, TRANSFER_TYPE_BULK, EP_DIR_OUT, 0, us_PipeOutCallback, us_BulkOut);
        if (result.IsFailure()) {
            Printf("no bulk/out endpoint present");
            return result;
        }

        // Now create SCSI disks for all LUN's here
        for (unsigned int lun = 0; lun <= us_max_lun; lun++) {
            ResourceSet sub_resourceSet;
            sub_resourceSet.AddResource(Resource(Resource::RT_ChildNum, lun, 0));

            Device* sub_device = device_manager::CreateDevice(
                "scsidisk", CreateDeviceProperties(*this, sub_resourceSet));
            if (sub_device == nullptr)
                continue;
            device_manager::AttachSingle(*sub_device);
        }

        return Result::Success();
    }

    Result USBStorage::Detach()
    {
        if (us_Device == nullptr)
            return Result::Success();

        if (us_BulkIn != nullptr)
            us_Device->FreePipe(*us_BulkIn);
        us_BulkIn = nullptr;

        if (us_BulkOut != nullptr)
            us_Device->FreePipe(*us_BulkOut);
        us_BulkOut = nullptr;
        return Result::Success();
    }

    struct USBStorage_Driver : public Driver {
        USBStorage_Driver() : Driver("usbstorage") {}

        const char* GetBussesToProbeOn() const override { return "usbbus"; }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override
        {
            auto res = cdp.cdp_ResourceSet.GetResource(Resource::RT_USB_Device, 0);
            if (res == nullptr)
                return nullptr;

            auto usb_dev = static_cast<usb::USBDevice*>(reinterpret_cast<void*>(res->r_Base));

            auto& iface = usb_dev->ud_interface[usb_dev->ud_cur_interface];
            if (iface.if_class == USB_IF_CLASS_STORAGE &&
                iface.if_protocol == USB_IF_PROTOCOL_BULKONLY)
                return new USBStorage(cdp);
            return nullptr;
        }
    };

    const RegisterDriver<USBStorage_Driver> registerDriver;

} // unnamed namespace
