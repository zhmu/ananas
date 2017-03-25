#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <ananas/driver.h>

void console_putchar(int c);
void console_putstring(const char* s);
uint8_t console_getchar();

extern Ananas::Device* console_tty;

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
	 : Driver(name), c_Priority(priority), c_Flags(flags)
	{
	}
	virtual ~ConsoleDriver() = default;

	ConsoleDriver* GetConsoleDriver() override
	{
		return this;
	}

	virtual Device* ProbeDevice() = 0;

	int c_Priority;
#define CONSOLE_FLAG_IN		0x0001
#define CONSOLE_FLAG_OUT	0x0002
#define CONSOLE_FLAG_INOUT	(CONSOLE_FLAG_IN | CONSOLE_FLAG_OUT)
	int c_Flags;
};

} // namespace Ananas

#endif /* __CONSOLE_H__ */
