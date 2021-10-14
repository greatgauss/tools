#ifndef NETUTILS_H
#define NETUTILS_H

#include <arpa/inet.h> //for in_addr_t


#define GOTO(ret_val, lbl) do {ret = ret_val; goto lbl;} while(0)

#define ERR_GOTO(expression, ret_val, lbl) \
    do { \
        if(expression) { \
            ret = ret_val; \
            goto lbl;\
        }\
    } while(0)


#define ERR_GOTO_WITH_INFO(expression, ret_val, lbl, info) \
    do { \
        if(expression) { \
            ret = ret_val; \
            printf("%s:%s\n", __FUNCTION__, info);\
            goto lbl;\
        }\
    } while(0)
int create_raw_socket
(char const *ifname, unsigned short type, unsigned char *hwaddr);


char *ifm_ipaddr(unsigned addr, char *addr_str);

int ifm_get_info
(const char *name, in_addr_t *addr, in_addr_t *mask, unsigned *flags);


int netutils_is_match_ip_packet
(unsigned char *packet, int pktsize,
unsigned short proto, unsigned int sip, unsigned int dip,
unsigned short sport, unsigned short dport);

#endif

