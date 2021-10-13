/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2021 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/lock.h"
#include "kernel/condvar.h"
#include "kernel/lib.h"
#include "kernel/device.h"
#include "kernel/dev/inputmux.h"
#include "kernel/init.h"
#include <ananas/util/array.h>
#include <ananas/inputmux.h>
#include <ananas/flags.h>
#include <ananas/util/ring_buffer.h>
#include "kernel/vfs/types.h"

namespace input_mux
{
    namespace
    {
        Mutex mutex{"inputmux"};

        util::List<IInputConsumer> consumers;

    } // unnamed namespace

    void RegisterConsumer(IInputConsumer& consumer)
    {
        MutexGuard g(mutex);
        consumers.push_back(consumer);
    }

    void UnregisterConsumer(IInputConsumer& consumer)
    {
        MutexGuard g(mutex);
        consumers.remove(consumer);
    }

    void OnEvent(const AIMX_EVENT& event)
    {
        MutexGuard g(mutex);
        for (auto& consumer : consumers)
            consumer.OnEvent(event);
    }

    namespace
    {
        inline constexpr auto eventQueueSize = 32;

        class InputMux : public Device, IDeviceOperations, ICharDeviceOperations, IInputConsumer
        {
          public:
            InputMux()
                : mutex{"inputmux"}, dataAvailable{"inputmux"}
            {
                d_Major = device_manager::AllocateMajor();
                d_Unit = 0;
                strcpy(d_Name, "inputmux");
            }
            virtual ~InputMux() = default;

            IDeviceOperations& GetDeviceOperations() override { return *this; }
            ICharDeviceOperations* GetCharDeviceOperations() override { return this; }

            Result Attach() override {
                RegisterConsumer(*this);
                return Result::Success();
            }
            Result Detach() override {
                UnregisterConsumer(*this);
                return Result::Success();
            }

            Result Open(Process* p) override
            {
                // Flush the queue on opens to avoid old stuff getting in the
                // way. This assumes there's only a single consumer at a time
                MutexGuard g(mutex);
                event_queue.clear();
                return Result::Success();
            }

            Result Read(VFS_FILE& file, void* buf, size_t len) override
            {
                mutex.Lock();
                while(true) {
                    if (!event_queue.empty()) {
                        const auto& event = event_queue.front();
                        const auto chunk = len < sizeof(event) ? len : sizeof(event);
                        memcpy(buf, &event, chunk);
                        event_queue.pop_front();
                        mutex.Unlock();

                        return Result::Success(chunk);
                    }

                    if (file.f_flags & O_NONBLOCK) {
                        mutex.Unlock();
                        return Result::Failure(EAGAIN);
                    }

                    dataAvailable.Wait(mutex);
                }
                // NOTREACHED
            }

            bool CanRead() override
            {
                MutexGuard g(mutex);
                return !event_queue.empty();
            }

            Result Write(VFS_FILE& file, const void* buffer, size_t len) override
            {
                // Do not accept anything send to us just yet
                return Result::Success(0);
            }

            void OnEvent(const AIMX_EVENT& event) override
            {
                MutexGuard g(mutex);
                //kprintf("OnEvent %d / %d, empty %d full %d\n", (int)event_queue.size(), (int)event_queue.capacity(), !!event_queue.empty(), !!event_queue.full());
                //if (event_queue.size() == 32) { kprintf("inputmux full!\n"); return;}
                event_queue.push_back(event);
                dataAvailable.Broadcast();
            }

            Mutex mutex;
            ConditionVariable dataAvailable;
            util::ring_buffer<AIMX_EVENT, eventQueueSize> event_queue;
        };
    }

    const init::OnInit init(init::SubSystem::Device, init::Order::Last, []() {
        device_manager::AttachSingle(*new InputMux);
    });
} // namespace input_mux
