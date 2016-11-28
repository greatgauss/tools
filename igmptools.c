#include <sys/types.h>  /* for type definitions */
#include <sys/socket.h> /* for socket API calls */
#include <netinet/in.h> /* for address structs */
#include <arpa/inet.h>  /* for sockaddr_in */
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() */
#include <string.h>     /* for strlen() */
#include <unistd.h>     /* for close() */
#include <errno.h>

#include "netutils.h"

#define MICRO (1000*1000)


//Open socket for multicast.
int open_mc_socket(int bind_port)
{
    int sock_fd = -1;
    int val = 1;
    int ret;
    struct sockaddr_in bind_addr;
    
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if ( sock_fd < 0 ) {
        fprintf(stderr, "create socket failed: %s(%d)\n", strerror(errno),errno);
        return -1;
    }
    
    ret = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&val, sizeof(val));

    memset(&bind_addr, 0, sizeof(struct sockaddr_in));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(bind_port);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ret = bind(sock_fd, (struct sockaddr *) &bind_addr, sizeof(bind_addr));
    if (ret < 0){
        fprintf(stderr, "bind socket to ANYADDR:%d failed: %s(%d)\n", bind_port, strerror(errno),errno);
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}


int close_mc_socket(int sock_fd)
{
    return close(sock_fd);
}


//group_addr: group address to join
//multi_if_addr: address of the interface from which the group is joined in
int join_group(int fd, const char* group_addr, const char* multi_if_addr)
{
    struct ip_mreq   mreq;
    in_addr_t addr;
    int ret;

    fprintf(stderr, "%s: group[%s], if_addr[%s])\n", __FUNCTION__, group_addr, multi_if_addr);

    addr = inet_addr(multi_if_addr);
    ret = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&addr, sizeof(in_addr_t));
    if (ret < 0){
        fprintf(stderr, "failed to set MULTICAST_IF: %s(%d)\n", strerror(errno),errno);
        return ret;
    }

    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(group_addr);
    mreq.imr_interface.s_addr = addr;

    ret = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    if (ret < 0){
        fprintf(stderr, "failed to set ADD_MEMBERSHIP: %s(%d)\n", strerror(errno),errno);
        return -1;
    }

    return 0;
}


//group_addr: group address to leave
//multi_if_addr: address of the interface from which the group is left from
int leave_group(int fd, const char* group_addr, const char* multi_if_addr)
{
    struct ip_mreq   mreq;
    in_addr_t addr;
    int ret;

    fprintf(stderr, "%s: group[%s], if_addr[%s])\n", __FUNCTION__, group_addr, multi_if_addr);

    addr = inet_addr(multi_if_addr);
    ret = setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, (char*)&addr, sizeof(in_addr_t));
    if (ret < 0){
        fprintf(stderr, "failed to set MULTICAST_IF: %s(%d)\n", strerror(errno),errno);
        return ret;
    }

    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(group_addr);
    mreq.imr_interface.s_addr = addr;

    ret = setsockopt(fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
    if (ret < 0){
        fprintf(stderr, "failed to set DROP_MEMBERSHIP: %s(%d)\n", strerror(errno),errno);
        return -1;
    }

    return 0;
}


static void usage( char * cmd)
{
    fprintf(stderr, "Join into or leave from a multicast group.\n");
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "%s join <InterfaceName> <GroupIP> <port>\n",cmd);
    fprintf(stderr, "%s leave <InterfaceName> <GroupIP> <port>\n",cmd);
    fprintf(stderr, "%s joinleave <InterfaceName> <GroupIP> <port> <counter>\n",cmd);
    return;
}

int main(int argc, char *argv[]) 
{
    char group_addr_str[32] = {0};   /* multicast IP address */
    char tmp_str[32] = {0};   /* multicast IP address */
    char if_addr_str[32] = {0};
    char *p_group_addr;             /* multicast IP address */
    char *p_multi_if_addr;
    unsigned short mc_port;       /* multicast port */
    unsigned short counter;
    int i;
    int fd;
    char *p;
    int timeout_in_seconds = 30;
    
    /* validate number of arguments */
    if (argc < 3) {
        usage(argv[0]);
        exit(1);
    }

    struct in_addr addr;
    struct in_addr net;

    ifm_get_info(argv[2], &addr.s_addr, &net.s_addr, NULL);
    ifm_ipaddr(*(unsigned int*)&addr, if_addr_str);
    p_multi_if_addr = if_addr_str;
    
    if (0 == strcmp(argv[1], "join") && argc >= 5) {
        p_group_addr = argv[3];
        mc_port = atoi(argv[4]);

        fd = open_mc_socket(mc_port);
        if (fd < 0) {
            fprintf(stderr, "failed to open multicast socket\n");
            return -1;
        }
        
        join_group(fd, p_group_addr, p_multi_if_addr);
        if (argc > 5)
            timeout_in_seconds = atoi(argv[5]);
        usleep(timeout_in_seconds * MICRO);

        close_mc_socket(fd);
        return 0;
    }
    else if (0 == strcmp(argv[1], "leave") && argc >= 4) {
        p_group_addr = argv[3];
        mc_port = atoi(argv[4]);

        fd = open_mc_socket(mc_port);
        if (fd < 0) 
            return -1;

        leave_group(fd, p_group_addr, p_multi_if_addr);
        close_mc_socket(fd);
        return 0;
    }
    else if (0 == strcmp(argv[1], "joinleave") && argc >= 6) {
        counter = atoi(argv[5]);
        if (counter > 254)
            counter = 254;
        p_group_addr = argv[3];
        mc_port = atoi(argv[4]);

        p = strrchr(p_group_addr, '.');
        memcpy(tmp_str, p_group_addr, p - p_group_addr);

        fd = open_mc_socket(mc_port);
        if (fd < 0) 
            return -1;

        for (i = 1; i <= counter; i++) {
            sprintf(group_addr_str, "%s.%d", tmp_str, i);

            fprintf(stderr, "ADD  : %s\n",group_addr_str);
            join_group(fd, group_addr_str, p_multi_if_addr);
            usleep(1*MICRO);

            fprintf(stderr, "LEAVE: %s\n",group_addr_str);
            leave_group(fd, group_addr_str, p_multi_if_addr);
            usleep(1*MICRO);

        }
        close_mc_socket(fd);
        return 0;

    }
    return 0;
}


