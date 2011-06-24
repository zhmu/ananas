#ifndef __ANANAS_USB_PIPE_H__
#define __ANANAS_USB_PIPE_H__

struct USB_DEVICE;
struct USB_PIPE;
struct USB_TRANSFER;
struct USB_ENDPOINT;

typedef void (*usb_pipe_callback_t)(struct USB_PIPE*);

struct USB_PIPE {
	struct USB_DEVICE* p_dev;
	struct USB_TRANSFER* p_xfer;
	struct USB_ENDPOINT* p_ep;
	usb_pipe_callback_t p_callback;
	
	/* Provide queue structure */
	DQUEUE_FIELDS(struct USB_PIPE);
};

DQUEUE_DEFINE(USB_PIPES, struct USB_PIPE);

errorcode_t usb_pipe_alloc(struct USB_DEVICE* usb_dev, int num, int type, int dir, usb_pipe_callback_t callback, struct USB_PIPE** pipe);
void usb_pipe_free(struct USB_PIPE** pipe);
errorcode_t usb_pipe_start(struct USB_PIPE* pipe);

#endif /* __ANANAS_USB_PIPE_H__ */
