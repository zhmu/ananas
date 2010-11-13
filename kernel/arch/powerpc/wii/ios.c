#include <ananas/types.h>
#include <ananas/wii/ipc.h>
#include <ananas/wii/ios.h>
#include <ananas/wii/invalidate.h>
#include <ananas/lock.h>
#include <ananas/lib.h>

static char open_devname[32] __attribute__((aligned(32)));

static struct SPINLOCK spl_ios_fname;

int
ios_open(const char* fname, int mode)
{
	KASSERT(strlen(fname) < sizeof(open_devname), "fname '%s' too long", fname);

	spinlock_lock(&spl_ios_fname);
	strcpy(open_devname, fname);
	invalidate_after_write(open_devname, strlen(open_devname) + 1);

	struct IPC_REQUEST* req = ipc_alloc_request();
	req->cmd = IOS_CMD_OPEN;
	req->args.open.name = open_devname;
	req->args.open.mode = mode;
	ipc_queue_request(req);
	int retval = ipc_wait_request(req);
	ipc_free_request(req);

	spinlock_unlock(&spl_ios_fname);
	return retval;
}

int
ios_close(int fd)
{
	struct IPC_REQUEST* req = ipc_alloc_request();
	req->cmd = IOS_CMD_CLOSE;
	req->fd = fd;
	ipc_queue_request(req);
	int retval = ipc_wait_request(req);
	ipc_free_request(req);

	return retval;
}

int
ios_read(int fd, void* buf, uint32_t size)
{
	struct IPC_REQUEST* req = ipc_alloc_request();
	req->cmd = IOS_CMD_READ;
	req->fd = fd;
	req->args.read.buffer = buf;
	req->args.read.length = size;
	ipc_queue_request(req);
	int retval = ipc_wait_request(req);
	ipc_free_request(req);

	if (buf != NULL)
		invalidate_for_read(buf, size);

	return retval;
}

int
ios_write(int fd, const void* buf, uint32_t size)
{
	if (buf != NULL)
		invalidate_after_write((void*)buf, size);

	struct IPC_REQUEST* req = ipc_alloc_request();
	req->cmd = IOS_CMD_WRITE;
	req->fd = fd;
	req->args.write.buffer = buf;
	req->args.write.length = size;
	ipc_queue_request(req);
	int retval = ipc_wait_request(req);
	ipc_free_request(req);

	return retval;
}

int
ios_seek(int fd, uint32_t pos, uint32_t whence)
{
	struct IPC_REQUEST* req = ipc_alloc_request();
	req->cmd = IOS_CMD_SEEK;
	req->fd = fd;
	req->args.seek.pos = pos;
	req->args.seek.whence = whence;
	ipc_queue_request(req);
	int retval = ipc_wait_request(req);
	ipc_free_request(req);

	return retval;
}

int
ios_ioctl(int fd, uint32_t ioctl, const void* in, int in_len, void* out, int out_len)
{
	struct IPC_REQUEST* req = ipc_alloc_request();

	if (in != NULL)
		invalidate_after_write((void*)in, in_len);
	if (out != NULL)
		invalidate_after_write(out, out_len);

	req->cmd = IOS_CMD_IOCTL;
	req->fd = fd;
	req->args.ioctl.ioctl = ioctl;
	req->args.ioctl.addr1 = (void*)in;
	req->args.ioctl.len1  = in_len;
	req->args.ioctl.addr2 = out;
	req->args.ioctl.len2  = out_len;
	ipc_queue_request(req);
	int retval = ipc_wait_request(req);
	ipc_free_request(req);

	if (out != NULL)
		invalidate_for_read(out, out_len);

	return retval;
}

int
ios_ioctlv(int fd, uint32_t ioctl, uint32_t num_in, uint32_t num_out, struct ioctlv* argv)
{
	struct IPC_REQUEST* req = ipc_alloc_request();

	for (int i = 0; i < num_in + num_out; i++) {
		invalidate_after_write(argv[i].buffer, argv[i].length);
	}
	invalidate_after_write(argv, (num_in + num_out) * sizeof(struct ioctlv));

	req->cmd = IOS_CMD_IOCTLV;
	req->fd = fd;
	req->args.ioctlv.ioctl   = ioctl;
	req->args.ioctlv.num_in  = num_in;
	req->args.ioctlv.num_out = num_out;
	req->args.ioctlv.argv    = argv;
	ipc_queue_request(req);
	int retval = ipc_wait_request(req);
	ipc_free_request(req);

	for (int i = 0; i < num_in + num_out; i++) {
		invalidate_for_read(argv[i].buffer, argv[i].length);
	}
	return retval;
}

void
ios_init()
{
	spinlock_init(&spl_ios_fname);
}

/* vim:set ts=2 sw=2: */
