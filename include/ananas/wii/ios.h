#ifndef __ANANAS_WII_IOS_H__
#define __ANANAS_WII_IOS_H__

#define IOS_CMD_OPEN   1
#define IOS_CMD_CLOSE  2
#define IOS_CMD_READ   3
#define IOS_CMD_WRITE  4
#define IOS_CMD_SEEK   5
#define IOS_CMD_IOCTL  6
#define IOS_CMD_IOCTLV 7

#define IOS_OPEN_MODE_R 1
#define IOS_OPEN_MODE_W 2
#define IOS_OPEN_MODE_RW (IOS_OPEN_MODE_R|IOS_OPEN_MODE_W)

struct ioctlv {
	void* buffer;
	uint32_t length;
};

int ios_open(const char* fname, int mode);
int ios_close(int fd);
int ios_read(int fd, void* buf, uint32_t size);
int ios_write(int fd, const void* buf, uint32_t size);
int ios_seek(int fd, uint32_t pos, uint32_t whence);
int ios_ioctl(int fd, uint32_t ioctl, const void* in, int in_len, void* out, int out_len);
int ios_ioctlv(int fd, uint32_t ioctl, uint32_t num_in, uint32_t numout, struct ioctlv* argv);

#endif /* __ANANAS_WII_IOS_H__ */
