#include "kernel/lock.h"
#include "kernel/lib.h"
#include "kernel/dev/kbdmux.h"

namespace keyboard_mux
{
    namespace
    {
        Mutex mutex{"kbdmutex"};

        util::List<IKeyboardConsumer> consumers;

    } // unnamed namespace

    void RegisterConsumer(IKeyboardConsumer& consumer)
    {
        MutexGuard g(mutex);
        consumers.push_back(consumer);
    }

    void UnregisterConsumer(IKeyboardConsumer& consumer)
    {
        MutexGuard g(mutex);
        consumers.remove(consumer);
    }

    void OnKey(const Key& key, int modifiers)
    {
        MutexGuard g(mutex);
        for (auto& consumer : consumers)
            consumer.OnKey(key, modifiers);
    }

} // namespace keyboard_mux

/* vim:set ts=2 sw=2: */
