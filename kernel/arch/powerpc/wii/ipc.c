#include <ananas/types.h>
#include <ananas/device.h>
#include <ananas/lock.h>
#include <ananas/lib.h>
#include <ananas/queue.h>
#include <ananas/mm.h>
#include <ananas/vm.h>
#include <ananas/wii/ipc.h>
#include <ananas/wii/ios.h>
#include <ananas/wii/invalidate.h>
#include <machine/param.h>

#define DEBUG

QUEUE_DEFINE(IPC_REQUEST_QUEUE, struct IPC_REQUEST);

/* Round requests to 32 bytes */
#define IPC_REQUEST_SIZE ((sizeof(struct IPC_REQUEST) | 0x1f) + 1)

static volatile void* IPC = (volatile void*)0xCD000000;
static spinlock_t spl_ipc_hw = SPINLOCK_DEFAULT_INIT;
static spinlock_t spl_ipc_items = SPINLOCK_DEFAULT_INIT;
static struct IPC_REQUEST_QUEUE ipc_requests;

#define IOS_BUFFER_BASE 0x133e0000
#define IOS_BUFFER_SIZE 0x20000

typedef volatile uint16_t vuint16_t;
typedef volatile uint32_t vuint32_t;

static uint8_t ipc_request_inuse[IOS_BUFFER_SIZE / IPC_REQUEST_SIZE];

void
ipc_interrupt()
{
	uint32_t status;

	status = *(vuint32_t*)(IPC + IPC_PPCCTRL);
	if ((status & (IPC_PPCCTRL_IYI2 | IPC_PPCCTRL_Y2)) == (IPC_PPCCTRL_IYI2 | IPC_PPCCTRL_Y2)) {
		/* Acknowledge */
		DEBUG("ipc_irq: ack\n");
		*(vuint32_t*)(IPC + IPC_PPCCTRL) = (status & IPC_PPCTRL_IOS_MASK) | IPC_PPCCTRL_IOS_CMD_ACK;
		*(vuint32_t*)(IPC + IPC_ISR) = (1 << 30); /* XXX why is this needed? */
	}

	status = *(vuint32_t*)(IPC + IPC_PPCCTRL);
	if ((status & (IPC_PPCCTRL_IYI1 | IPC_PPCCTRL_Y1)) == (IPC_PPCCTRL_IYI1 | IPC_PPCCTRL_Y1)) {
		/* Reply */
		struct IPC_REQUEST* req = (struct IPC_REQUEST*)(*(vuint32_t*)(IPC + IPC_ARMMSG));
		KASSERT(QUEUE_HEAD(&ipc_requests) == req, "top of queue not handled!");
		invalidate_for_read(req, 32); /* flush cache */

		QUEUE_POP_HEAD(&ipc_requests);
		DEBUG("ipc_interrupt(): got req=%x\n", req);
		KASSERT(req->magic == IPC_REQUEST_MAGIC, "ipc request corrupt");
		req->magic = 0;

		status = (*(vuint32_t*)(IPC + IPC_PPCCTRL)) & IPC_PPCTRL_IOS_MASK;
		*(vuint32_t*)(IPC + IPC_PPCCTRL) = status | IPC_PPCCTRL_IOS_EXECUTED;
		*(vuint32_t*)(IPC + IPC_ISR) = (1 << 30); /* XXX why is this needed? */

		DEBUG("req: cmd=%u, ret=0x%x, fd=0x%x\n", req->cmd, req->ret, req->fd);
		req->completed++;
		
		/* End of interrupt sequence */
		status = (*(vuint32_t*)(IPC + IPC_PPCCTRL)) & IPC_PPCTRL_IOS_MASK;
		*(vuint32_t*)(IPC + IPC_PPCCTRL) = status | IPC_PPCCTRL_X2;
	}
}

static void
ipc_send_request(struct IPC_REQUEST* req)
{
	DEBUG("ipc_send_request(): req=%x\n", req);
	invalidate_after_write(req, 32); /* flush cache */
	*(vuint32_t*)(IPC + IPC_PPCMSG) = (uint32_t)req;
	uint32_t ctrl = *(vuint32_t*)(IPC + IPC_PPCCTRL);
	*(vuint32_t*)(IPC + IPC_PPCCTRL) = (ctrl & IPC_PPCTRL_IOS_MASK) | IPC_PPCCTRL_IOS_EXEC;
}

void
ipc_queue_request(struct IPC_REQUEST* req)
{
	DEBUG("ipc_queue_request(): req=%x\n", req);

	spinlock_lock(&spl_ipc_items);
	int queue_empty = QUEUE_EMPTY(&ipc_requests);
	QUEUE_ADD_TAIL(&ipc_requests, req);
	if (queue_empty)
		ipc_send_request(req);
	spinlock_unlock(&spl_ipc_items);
}

int
ipc_wait_request(struct IPC_REQUEST* req)
{
	DEBUG("ipc_wait_request(): req=%x\n", req);
	while(!req->completed) {
		/* Now, we wait... XXX reschedule? */
	}
	return req->ret;
}

struct IPC_REQUEST*
ipc_alloc_request()
{
	spinlock_lock(&spl_ipc_items);
	for (int i = 0; i < IOS_BUFFER_SIZE / IPC_REQUEST_SIZE; i++) {
		if ((ipc_request_inuse[i / 8] & (1 << (i & 7))) == 0) {
			ipc_request_inuse[i / 8] |= 1 << (i & 7);
			spinlock_unlock(&spl_ipc_items);

			struct IPC_REQUEST* req = (struct IPC_REQUEST*)(IOS_BUFFER_BASE + (i * IPC_REQUEST_SIZE));
			memset(req, 0, sizeof(*req));
			req->magic = IPC_REQUEST_MAGIC;
			return req;
		}
	}
	spinlock_unlock(&spl_ipc_items);
	return NULL;
}

void
ipc_free_request(struct IPC_REQUEST* req)
{
	spinlock_lock(&spl_ipc_items);
	KASSERT((((addr_t)req >= IOS_BUFFER_BASE) && (addr_t)req < IOS_BUFFER_BASE + IOS_BUFFER_SIZE), "request 0x%x out of range", req);
	uint32_t req_no = ((addr_t)req - IOS_BUFFER_BASE) / IPC_REQUEST_SIZE;
	KASSERT((ipc_request_inuse[req_no / 8] & (1 << (req_no & 7))), "request not in use");
	ipc_request_inuse[req_no / 8] &= ~(1 << (req_no & 7));
	spinlock_unlock(&spl_ipc_items);
}

void
ipc_init()
{
	KASSERT(sizeof(struct IPC_REQUEST) <= IPC_REQUEST_SIZE, "IPC_REQUEST_SIZE too small");

	QUEUE_INIT(&ipc_requests);
	memset(&ipc_request_inuse, 0, IOS_BUFFER_SIZE / IPC_REQUEST_SIZE);

	/*
	 * Map the IOS memory specifically reserved for the requests; this is
	 * necessary because the Starlet isn't very happy about doing non-32 bit 
	 * writes to the lower 24MB...
	 */
	/* XXX this relies on vm_map_kernel() using 1:1 mappings */
	vm_map_kernel(IOS_BUFFER_BASE, IOS_BUFFER_SIZE / PAGE_SIZE, VM_FLAG_READ | VM_FLAG_WRITE);
}

/* vim:set ts=2 sw=2: */
