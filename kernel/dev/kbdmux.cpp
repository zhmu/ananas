#include <ananas/device.h>
#include <ananas/console.h>
#include <ananas/error.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/mm.h>
#include <ananas/tty.h>

#define KBDMUX_BUFFER_SIZE 16

struct KBDMUX_PRIVDATA {
	/* We must use a spinlock here as kbdmux_on_input() can be called from IRQ context */
	spinlock_t kbd_lock;
	char kbd_buffer[KBDMUX_BUFFER_SIZE];
	int kbd_buffer_readpos;
	int kbd_buffer_writepos;
};

static struct KBDMUX_PRIVDATA* kbdmux_privdata = NULL;

void
kbdmux_on_input(uint8_t ch)
{
	struct KBDMUX_PRIVDATA* priv = kbdmux_privdata;
	KASSERT(priv != NULL, "input without priv");

	/* Add the data to our buffer */
	register_t state = spinlock_lock_unpremptible(&priv->kbd_lock);
	/* XXX we should protect against buffer overruns */
	priv->kbd_buffer[priv->kbd_buffer_writepos] = ch;
	priv->kbd_buffer_writepos = (priv->kbd_buffer_writepos + 1) % KBDMUX_BUFFER_SIZE;
	spinlock_unlock_unpremptible(&priv->kbd_lock, state);

	/* XXX signal consumers - this is a hack */
	if (console_tty != NULL) {
		tty_signal_data();
	}
}

static errorcode_t
kbdmux_read(device_t dev, void* data, size_t* len, off_t off)
{
	auto priv = static_cast<struct KBDMUX_PRIVDATA*>(dev->privdata);
	size_t returned = 0, left = *len;

	register_t state = spinlock_lock_unpremptible(&priv->kbd_lock);
	while (left-- > 0) {
		if (priv->kbd_buffer_readpos == priv->kbd_buffer_writepos)
			break;

		*(uint8_t*)(static_cast<uint8_t*>(data) + returned++) = priv->kbd_buffer[priv->kbd_buffer_readpos];
		priv->kbd_buffer_readpos = (priv->kbd_buffer_readpos + 1) % KBDMUX_BUFFER_SIZE;
	}
	spinlock_unlock_unpremptible(&priv->kbd_lock, state);

	*len = returned;
	return ananas_success();
}

static errorcode_t
kbdmux_attach(device_t dev)
{
	auto mux_priv = static_cast<struct KBDMUX_PRIVDATA*>(kmalloc(sizeof(struct KBDMUX_PRIVDATA)));
	memset(mux_priv, 0, sizeof(*mux_priv));
	dev->privdata = mux_priv;
	spinlock_init(&mux_priv->kbd_lock);

	KASSERT(kbdmux_privdata == NULL, "multiple kbdmux?");
	kbdmux_privdata = mux_priv;
	return ananas_success();
}

struct DRIVER drv_kbdmux = {
	.name       = "kbdmux",
	.drv_attach	= kbdmux_attach,
	.drv_read   = kbdmux_read
};

DRIVER_PROBE(kbdmux)
DRIVER_PROBE_BUS(corebus)
DRIVER_PROBE_END()

DEFINE_CONSOLE_DRIVER(drv_kbdmux, 10, CONSOLE_FLAG_IN)

/* vim:set ts=2 sw=2: */
