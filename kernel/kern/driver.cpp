#include <ananas/types.h>
#include <ananas/error.h>
#include <ananas/trace.h>
#include <ananas/device.h>
#include <ananas/driver.h>
#include <ananas/lib.h>

namespace Ananas {

bool Driver::MustProbeOnBus(const Device& bus) const
{
	const char* ptr = GetBussesToProbeOn();
	if (ptr == nullptr)
		return false;

	size_t bus_name_len = strlen(bus.d_Name);
	while(*ptr != '\0') {
		const char* next = strchr(ptr, ',');
		if (next == NULL)
			next = strchr(ptr, '\0');

		size_t len = next - ptr;
		if (bus_name_len == len && strncmp(ptr, bus.d_Name, len) == 0) {
			return true;
		}

		if (*next == '\0')
			break;

		ptr = next + 1;
	}
	return false;
}

} // namespace Ananas

/* vim:set ts=2 sw=2: */
