#ifndef __UDP_H__
#define __UDP_H__

int udp_open(uint32_t ip, uint16_t port);
void udp_close();
int udp_write(const void* buffer, size_t length);
size_t udp_read(void* buffer, size_t length);


#endif /* __UDP_H__ */
