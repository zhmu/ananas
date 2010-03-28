/*
 * Implementation of our TTY device; this multiplexes input/output devices to a
 * single device, and handles the TTY magic.
 */
#include <sys/types.h>
#include <sys/device.h>
#include <sys/mm.h>

struct TTY_PRIVDATA {
	device_t	input_dev;
	device_t	output_dev;
};

static struct DRIVER drv_tty;

device_t
tty_alloc(device_t input_dev, device_t output_dev)
{
	device_t dev = device_alloc(NULL, &drv_tty);
	if (dev == NULL)
		return NULL;

	struct TTY_PRIVDATA* priv = kmalloc(sizeof(struct TTY_PRIVDATA));
	priv->input_dev = input_dev;
	priv->output_dev = output_dev;
	dev->privdata = priv;
	return dev;
}

device_t
tty_get_inputdev(device_t dev)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;
	return priv->input_dev;
}

device_t
tty_get_outputdev(device_t dev)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;
	return priv->output_dev;
}

static ssize_t
tty_write(device_t dev, const char* data, size_t len)
{
	struct TTY_PRIVDATA* priv = (struct TTY_PRIVDATA*)dev->privdata;
	return priv->output_dev->driver->drv_write(priv->output_dev, data, len);
}

static ssize_t
tty_read(device_t dev, char* data, size_t len)
{
	/* NOTYET */
	return 0;
}

static struct DRIVER drv_tty = {
	.name					= "tty",
	.drv_probe		= NULL,
	.drv_read			= tty_read,
	.drv_write		= tty_write
};

/* vim:set ts=2 sw=2: */
