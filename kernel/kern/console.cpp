#include <ananas/error.h>
#include "kernel/console.h"
#include "kernel/console-driver.h"
#include "kernel/debug-console.h"
#include "kernel/device.h"
#include "kernel/mm.h"
#include "kernel/lib.h"
#include "kernel/lock.h"
#include "kernel/tty.h"
#include "options.h"

namespace Ananas {
namespace DriverManager {
namespace internal {
Ananas::DriverList& GetDriverList();
} // namespace internal
} // namespace DriverManager
namespace DeviceManager {
namespace internal {
Device* ProbeConsole(ConsoleDriver& driver);
} // namespace internal
} // namespace DeviceManager
} // namespace Ananas

Ananas::Device* console_tty = nullptr;

#define CONSOLE_BACKLOG_SIZE 1024

static int console_mutex_inuse = 0;
static spinlock_t console_backlog_lock;
static char* console_backlog;
static int console_backlog_pos = 0;
static int console_backlog_overrun = 0;
static mutex_t mtx_console;

/* If set, display the driver list before attaching */
#define VERBOSE_LIST 0

static errorcode_t
console_init()
{

	Ananas::Device* input_dev = nullptr;
	Ananas::Device* output_dev = nullptr;
	Ananas::DriverList& drivers = Ananas::DriverManager::internal::GetDriverList();
	for(auto& driver: drivers) {
		Ananas::ConsoleDriver* consoleDriver = driver.GetConsoleDriver();
		if (consoleDriver == nullptr)
			continue; // not a console driver, skip

		/* Skip any driver that is already provided for */
		if (((consoleDriver->c_Flags & CONSOLE_FLAG_IN ) == 0 || input_dev  != nullptr) &&
		    ((consoleDriver->c_Flags & CONSOLE_FLAG_OUT) == 0 || output_dev != nullptr))
			continue;

		/*
		 * OK, see if this devices probes. Note that we don't provide any
		 * resources here - XXX devices that require them can't be used as
		 * console devices currently.
		 */
		Ananas::Device* dev = Ananas::DeviceManager::internal::ProbeConsole(*consoleDriver);
		if (dev == NULL)
			continue; // likely not present
		errorcode_t err = Ananas::DeviceManager::AttachSingle(*dev);
		if (ananas_is_failure(err)) {
			// Too bad; this driver won't work
			delete dev;
			continue;
		}

		/* We have liftoff! */
		if ((consoleDriver->c_Flags & CONSOLE_FLAG_IN) && input_dev == nullptr)
			input_dev = dev;
		if ((consoleDriver->c_Flags & CONSOLE_FLAG_OUT) && output_dev == nullptr)
			output_dev = dev;
	}

	if (input_dev != nullptr || output_dev != nullptr)
		console_tty = tty_alloc(input_dev, output_dev);

	/* Initialize the backlog; we use it to queue messages once the mutex is hold */
	spinlock_init(&console_backlog_lock);
	console_backlog = new char[CONSOLE_BACKLOG_SIZE];
	console_backlog_pos = 0;

	/* Initialize the console print mutex and start using it */
	mutex_init(&mtx_console, "console");
	console_mutex_inuse++;

	return ananas_success();
}

INIT_FUNCTION(console_init, SUBSYSTEM_CONSOLE, ORDER_MIDDLE);

void
console_putchar(int c)
{
#ifdef OPTION_DEBUG_CONSOLE
	debugcon_putch(c);
#endif /* OPTION_DEBUG_CONSOLE */
	if (console_tty == NULL)
		return;
	uint8_t ch = c; // cannot cast due to endianness!
	size_t len = sizeof(ch);

	console_tty->GetCharDeviceOperations()->Write((const void*)&ch, len, 0);
}

static void
console_puts(const char* s)
{
	for (int c = *s++; c != 0; c = *s++)
		console_putchar(c);
}

void
console_putstring(const char* s)
{
	if (console_mutex_inuse && !mutex_trylock(&mtx_console)) {
		/*
		 * Couldn't obtain the console mutex; we should put our message in the
		 * back log and wait for it to be picked up later.
		 */
		int len = strlen(s);
		register_t state = spinlock_lock_unpremptible(&console_backlog_lock);
		if (console_backlog_pos + len < CONSOLE_BACKLOG_SIZE) {
			/* Fits! */
			strcpy(&console_backlog[console_backlog_pos], s);
			console_backlog_pos += len;
		} else {
			/* Doesn't fit */
			console_backlog_overrun++;
		}
		spinlock_unlock_unpremptible(&console_backlog_lock, state);
		return;
	}

	console_puts(s);

	/* See if there's anything in the backlog that we can print */
	if (console_backlog_pos > 0) {
		register_t state = spinlock_lock_unpremptible(&console_backlog_lock);
		for (int n = 0; n < console_backlog_pos; n++)
			console_putchar(console_backlog[n]);
		console_backlog_pos = 0;

		if (console_backlog_overrun)
			console_puts("[***OVERRUN***]");
		spinlock_unlock_unpremptible(&console_backlog_lock, state);
	}


	if (console_mutex_inuse)
		mutex_unlock(&mtx_console);
}

uint8_t
console_getchar()
{
#ifdef OPTION_DEBUG_CONSOLE
	int ch = debugcon_getch();
	if (ch != 0)
		return ch;
#endif /* OPTION_DEBUG_CONSOLE */
	if (console_tty == NULL)
		return 0;
	uint8_t c;
	size_t len = sizeof(c);
	if (console_tty->GetCharDeviceOperations()->Read((void*)&c, len, 0) < 1)
		return 0;
	return c;
}

/* vim:set ts=2 sw=2: */
