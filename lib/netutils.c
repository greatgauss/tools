#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <linux/route.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>

#include <netdb.h>


#define NOT_UNICAST(e) ((e[0] & 0x01) != 0)
#define GOTO(ret_val, lbl) do {ret = ret_val; goto lbl;} while(0)   


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
    	    char buffer[256];
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




