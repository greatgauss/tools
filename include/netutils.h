#ifndef NETUTILS_H
#define NETUTILS_H

int create_raw_socket
(char const *ifname, unsigned short type, unsigned char *hwaddr);

#endif

