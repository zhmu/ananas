#ifndef __CONSOLEDRIVER_H__
#define __CONSOLEDRIVER_H__

#include "driver.h"

namespace Ananas {

/*
 * Console drivers are 'special' as they will be probed during early boot and
 * skipped afterwards.
 *
 * The priority influences the order in which these things are attached, the
 * first successful driver wins and becomes the console driver. The order is
 * highest first, so use zero for the 'if all else fails'-driver.
 */
class ConsoleDriver : public Driver
{
public:
	ConsoleDriver(const char* name, int priority, int flags)
	 : Driver(name, priority), c_Flags(flags)
	{
	}
	virtual ~ConsoleDriver() = default;

	ConsoleDriver* GetConsoleDriver() override
	{
		return this;
	}

	virtual Device* ProbeDevice() = 0;

#define CONSOLE_FLAG_IN		0x0001
#define CONSOLE_FLAG_OUT	0x0002
#define CONSOLE_FLAG_INOUT	(CONSOLE_FLAG_IN | CONSOLE_FLAG_OUT)
	int c_Flags;
};

} // namespace Ananas

#endif /* __CONSOLEDRIVER_H__ */
