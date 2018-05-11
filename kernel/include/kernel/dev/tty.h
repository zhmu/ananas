#ifndef ANANAS_DEV_TTY_H
#define ANANAS_DEV_TTY_H

#include <ananas/types.h>
#include <ananas/util/array.h>
#include <termios.h>
#include "kernel/device.h"

class TTY : public Ananas::Device, private Ananas::IDeviceOperations, private Ananas::ICharDeviceOperations
{
public:
	TTY(const Ananas::CreateDeviceProperties& cdp);
	virtual ~TTY() = default;

	IDeviceOperations& GetDeviceOperations() override
	{
		return *this;
	}

	ICharDeviceOperations* GetCharDeviceOperations() override
	{
		return this;
	}

	Result Read(void* buf, size_t& len, off_t offset) override;
	Result Write(const void* buffer, size_t& len, off_t offset) override;

	Result OnInput(const char* buffer, size_t len);

protected:
	virtual Result Print(const char* buffer, size_t len) = 0;

private:
	static constexpr size_t InputQueueSize = 256;

	void PutChar(unsigned char ch);
	void HandleEcho(unsigned char byte);

	struct termios tty_termios;
	util::array<char, InputQueueSize> tty_input_queue;
	unsigned int tty_in_writepos = 0;
	unsigned int tty_in_readpos = 0;
};

#endif /* ANANAS_DEV_TTY_H */
