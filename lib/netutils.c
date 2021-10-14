#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <linux/route.h>
#include <linux/ip.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include <netdb.h>

#include "netutils.h"

#define NOT_UNICAST(e) ((e[0] & 0x01) != 0)

static char dump_buf[8192];
void netutils_dump_data(char *desc, unsigned char *data, int datasize)
{
	int i, j, line;

	fprintf(stderr, "%s(%d bytes)\n", desc, datasize);

	for (i = 0, j = 0, line = 0; i < datasize; ++i) {
        j += sprintf(dump_buf + j, "%2.2X ", data[i]);
		if (i % 16 == 15) {
			fprintf(stderr, "%s\n", dump_buf);
			line++;
			j = 0;
		}
	}

	if (line * 16 < datasize) {
		fprintf(stderr, "%s\n", dump_buf);
	}
}

int netutils_is_match_ip_packet
(unsigned char *packet, int pktsize,
unsigned short proto, unsigned int sip, unsigned int dip, unsigned short sport, unsigned short dport)
{
    struct iphdr *ip;
    int hlen;
    unsigned char *tcpudp;

    (void)pktsize;
    packet += 14;  //skip ethernet header
    ip = (struct iphdr *)packet;

    if(ip->version != 4)
        return 0;
	hlen = ip->ihl << 2;

    if (proto != 0 && ip->protocol != proto) {
        return 0;
    }

    if (sip != 0 && ip->saddr != sip) {
        return 0;
    }

    if (dip != 0 && ip->daddr != dip) {
        return 0;
    }

    if (IPPROTO_UDP == ip->protocol || IPPROTO_TCP == ip->protocol) {
        tcpudp = packet + hlen * 4;
        if (sport != 0 && sport != ntohs(*(unsigned short*)tcpudp))
            return 0;
        if (dport != 0 && dport != ntohs(*(unsigned short*)(tcpudp+2)))
            return 0;
    }

    return 1;
}


int create_raw_socket
(char const *ifname, unsigned short type, unsigned char *hwaddr)
{
    int optval=1;
    int fd;
    struct ifreq ifr;
    int domain, stype;
    int ret;

    struct sockaddr_ll sa;

    memset(&sa, 0, sizeof(sa));

    domain = PF_PACKET;
    stype = SOCK_RAW;

    if ((fd = socket(domain, stype, htons(type))) < 0) {
        if (errno == EPERM) {
            fprintf(stderr, "Cannot create raw socket:%s\n", strerror(errno));
        }
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
        fprintf(stderr, "failed to setsockopt:%s\n", strerror(errno));
        GOTO(-1,exit_now);
    }

    /* Fill in hardware address */
    if (hwaddr) {
    	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
        if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) {
    	    fprintf(stderr, "failed to ioctl(SIOCGIFHWADDR)");
        GOTO(-1,exit_now);
    	}
    	memcpy(hwaddr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

    	if (NOT_UNICAST(hwaddr)) {
            fprintf(stderr, "Interface %.16s has broadcast/multicast MAC address??", ifname);
            GOTO(-1,exit_now);
    	}
    }

    /* Sanity check on MTU */
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(fd, SIOCGIFMTU, &ifr) < 0) {
    	fprintf(stderr, "failed to ioctl(SIOCGIFMTU):%s\n", strerror(errno));
    }


    /* Get interface index */
    sa.sll_family = AF_PACKET;
    sa.sll_protocol = htons(type);

    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
    	fprintf(stderr, "faled to ioctl(SIOCFIGINDEX): Could not get interface index:%s\n", strerror(errno));
        GOTO(-1,exit_now);
    }
    sa.sll_ifindex = ifr.ifr_ifindex;

    /* We're only interested in packets on specified interface */
    if (bind(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
    	fprintf(stderr, "failed to bind:%s\n", strerror(errno));
        GOTO(-1,exit_now);
    }

    return fd;

exit_now:
    close(fd);
    return ret;
}



static int ifm_ctl_sock = -1;



char *ifm_ipaddr(unsigned addr, char *addr_str)
{
    sprintf(addr_str,"%d.%d.%d.%d",
            addr & 255,
            ((addr >> 8) & 255),
            ((addr >> 16) & 255),
            (addr >> 24));
    return addr_str;
}


int ifm_open_ctrl_sock(void)
{
    if (ifm_ctl_sock == -1) {
        ifm_ctl_sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (ifm_ctl_sock < 0) {
            fprintf(stderr, "open socket failed: %s\n", strerror(errno));
        }
    }
    return ifm_ctl_sock < 0 ? -1 : 0;
}

void ifm_close_ctrl_sock(void)
{
    if (ifm_ctl_sock != -1) {
        (void)close(ifm_ctl_sock);
        ifm_ctl_sock = -1;
    }
}

static void ifm_init_ifreq(const char *name, struct ifreq *ifr)
{
    memset(ifr, 0, sizeof(struct ifreq));
    strncpy(ifr->ifr_name, name, IFNAMSIZ);
    ifr->ifr_name[IFNAMSIZ - 1] = 0;
}


//ASSUME IPv4???
int ifm_get_info
(const char *name,
in_addr_t *addr,
in_addr_t *mask,
unsigned *flags)
{
    struct ifreq ifr;

    ifm_open_ctrl_sock();
    ifm_init_ifreq(name, &ifr);

    if (addr != NULL) {
        if(ioctl(ifm_ctl_sock, SIOCGIFADDR, &ifr) < 0) {
            *addr = 0;
        } else {
            *addr = ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;
        }
    }

    if (mask != NULL) {
        if(ioctl(ifm_ctl_sock, SIOCGIFNETMASK, &ifr) < 0) {
            *mask = 0;
        } else {
            *mask = ((struct sockaddr_in*) &ifr.ifr_addr)->sin_addr.s_addr;
        }
    }

    if (flags != NULL) {
        if(ioctl(ifm_ctl_sock, SIOCGIFFLAGS, &ifr) < 0) {
            *flags = 0;
        } else {
            *flags = ifr.ifr_flags;
        }
    }

    ifm_close_ctrl_sock();
    return 0;
}


