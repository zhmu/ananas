#ifndef __TTY_H__
#define __TTY_H__

namespace Ananas {
class Device;
}

Ananas::Device* tty_alloc(Ananas::Device* input_dev, Ananas::Device* output_dev);
Ananas::Device* tty_get_inputdev(Ananas::Device*);
Ananas::Device* tty_get_outputdev(Ananas::Device*);
void tty_signal_data();

#endif /* __TTY_H__ */
