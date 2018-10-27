#ifndef __SHUTDOWN_H__
#define __SHUTDOWN_H__

namespace shutdown {

enum class ShutdownType {
	Unknown,
	Halt,
	Reboot,
	PowerDown
};

bool IsShuttingDown();
void RequestShutdown(ShutdownType type);

} // namespace shutdown

#endif // __SHUTDOWN_H__
