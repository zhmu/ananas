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
                q_readpos = q_writepos;
                return Result::Success();
            }

            Result Read(VFS_FILE& file, void* buf, size_t len) override
            {
                mutex.Lock();
                while(true) {
                    if (q_readpos != q_writepos) {
                        const auto event = event_queue[q_readpos];
                        q_readpos = (q_readpos + 1) % event_queue.size();
                        mutex.Unlock();

                        const auto chunk = len < sizeof(event) ? len : sizeof(event);
                        memcpy(buf, &event, chunk);
                        return Result::Success(chunk);
                    }

                    dataAvailable.Wait(mutex);
                }
                // NOTREACHED
            }

            bool CanRead() override
            {
                MutexGuard g(mutex);
                return q_readpos != q_writepos;
            }

            Result Write(VFS_FILE& file, const void* buffer, size_t len) override
            {
                // Do not accept anything send to us just yet
                return Result::Success(0);
            }

            void OnEvent(const AIMX_EVENT& event) override
            {
                MutexGuard g(mutex);
                event_queue[q_writepos] = event;
                q_writepos = (q_writepos + 1) % event_queue.size();
                dataAvailable.Broadcast();
            }

            Mutex mutex;
            ConditionVariable dataAvailable;
            util::array<AIMX_EVENT, 32> event_queue;
            unsigned int q_writepos = 0;
            unsigned int q_readpos = 0;
        };
    }

    const init::OnInit init(init::SubSystem::Device, init::Order::Last, []() {
        device_manager::AttachSingle(*new InputMux);
    });
} // namespace input_mux
