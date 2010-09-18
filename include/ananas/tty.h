#include <ananas/device.h>

#ifndef __TTY_H__
#define __TTY_H__

device_t tty_alloc(device_t input_dev, device_t output_dev);
device_t tty_get_inputdev(device_t dev);
device_t tty_get_outputdev(device_t dev);
void tty_signal_data(device_t dev);

#endif /* __TTY_H__ */
