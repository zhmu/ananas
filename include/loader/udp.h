#ifndef __UDP_H__
#define __UDP_H__

int udp_open();
void udp_close();
int udp_sendto(const void* buffer, size_t length, uint32_t ip, int16_t port);
size_t udp_recvfrom(void* buffer, size_t length, uint32_t* ip, uint16_t* port);


#endif /* __UDP_H__ */
