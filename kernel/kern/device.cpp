#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/lib.h>

namespace Ananas {

namespace DeviceManager {
namespace internal {
void Unregister(Device&);
} // namespace internal
} // namespace DeviceManager

Device::Device()
	: d_Parent(this)
{
	li_next = NULL; li_prev = NULL;
	sem_init(&d_Waiters, 1);
}

Device::Device(const CreateDeviceProperties& cdp)
	: d_Parent(cdp.cdp_Parent),
	  d_ResourceSet(cdp.cdp_ResourceSet)
{
	li_next = NULL; li_prev = NULL;
	sem_init(&d_Waiters, 1);
}

Device::~Device()
{
	DeviceManager::internal::Unregister(*this);
}

void Device::Printf(const char* fmt, ...) const
{
#define DEVICE_PRINTF_BUFSIZE 256
	char buf[DEVICE_PRINTF_BUFSIZE];

	snprintf(buf, sizeof(buf), "%s%u: ", d_Name, d_Unit);

	va_list va;
	va_start(va, fmt);
	vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf) - 2, fmt, va);
	buf[sizeof(buf) - 2] = '\0';
	strcat(buf, "\n");
	va_end(va);

	console_putstring(buf);
}

} // namespace Ananas
