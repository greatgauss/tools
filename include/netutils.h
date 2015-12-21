#ifndef NETUTILS_H
#define NETUTILS_H

#include <arpa/inet.h> //for in_addr_t


int create_raw_socket
(char const *ifname, unsigned short type, unsigned char *hwaddr);


char *ifm_ipaddr(unsigned addr, char *addr_str);

int ifm_get_info
(const char *name, in_addr_t *addr, in_addr_t *mask, unsigned *flags);


#endif

