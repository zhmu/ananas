#ifndef __ANANAS_WII_IPC_H__
#define __ANANAS_WII_IPC_H__

#include <ananas/queue.h>

#define IPC_PPCMSG 0x0

#define IPC_PPCCTRL 0x4
# define IPC_PPCCTRL_X1 (1 << 0)
# define IPC_PPCCTRL_Y2 (1 << 1)
# define IPC_PPCCTRL_Y1 (1 << 2)
# define IPC_PPCCTRL_X2 (1 << 3)
# define IPC_PPCCTRL_IYI1 (1 << 4)
# define IPC_PPCCTRL_IYI2 (1 << 5)

#define IPC_ARMMSG 0x8
#define IPC_ARMCTRL 0xc /* Note: cannot be accessed */

#define IPC_ISR 0x30

/* Execute command; pointer available in IPC_PPCMSG */
#define IPC_PPCCTRL_IOS_EXEC	IPC_PPCCTRL_X1

/* Command acknowledge */
#define IPC_PPCCTRL_IOS_CMD_ACK	IPC_PPCCTRL_Y2

/* Command executed; reply available in IPC_ARMMSG */
#define IPC_PPCCTRL_IOS_EXECUTED IPC_PPCCTRL_Y1

/* Register mask needed to send IOS commands */
#define IPC_PPCTRL_IOS_MASK (IPC_PPCCTRL_IYI1 | IPC_PPCCTRL_IYI2)

struct IPC_REQUEST {
	/* below is for the IOS starlet processor */
	uint32_t cmd;
	int32_t  ret;
	uint32_t fd;
	union {
		struct {
			const char* 	name;
			uint32_t	mode;
		} open;
		struct {
			void*		buffer;
			uint32_t	length;
		} read;
		struct {
			const void*	buffer;
			uint32_t	length;
		} write;
		struct {
			uint32_t	pos;
			uint32_t	whence;
		} seek;
		struct {
			uint32_t	ioctl;
			void*		addr1;
			uint32_t	len1;
			void*		addr2;
			uint32_t	len2;
		} ioctl;
		struct {
			uint32_t	ioctl;
			uint32_t	num_in;
			uint32_t	num_out;	/* or in-out */
			void*		argv;
		} ioctlv;
		uint32_t argv[5];
	} args;
	/* above is for the IOS starlet processor */
	int completed;
	/* Ensure we don't accept anything*/
	uint32_t magic;
#define IPC_REQUEST_MAGIC 0xabcd6780
	QUEUE_FIELDS;
} __attribute__((packed));

void ipc_init();
void ipc_interrupt();

struct IPC_REQUEST* ipc_alloc_request();
void ipc_free_request(struct IPC_REQUEST* req);

void ipc_queue_request(struct IPC_REQUEST* req);
int ipc_wait_request(struct IPC_REQUEST* req);

#endif /* __ANANAS_WII_IPC_H__ */
