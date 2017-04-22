#include <ananas/console.h>
#include <ananas/device.h>
#include <ananas/lib.h>

namespace Ananas {

namespace DeviceManager {
namespace internal {
void OnDeviceDestruction(Device&);
} // namespace internal
} // namespace DeviceManager

Device::Device()
	: d_Parent(this)
{
	all_next = NULL; all_prev = NULL;
	children_next = NULL; children_prev = NULL;
	sem_init(&d_Waiters, 1);
	LIST_INIT(&d_Children);
}

Device::Device(const CreateDeviceProperties& cdp)
	: d_Parent(cdp.cdp_Parent),
	  d_ResourceSet(cdp.cdp_ResourceSet)
{
	all_next = NULL; all_prev = NULL;
	children_next = NULL; children_prev = NULL;
	sem_init(&d_Waiters, 1);
	LIST_INIT(&d_Children);
}

Device::~Device()
{
	DeviceManager::internal::OnDeviceDestruction(*this);
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
