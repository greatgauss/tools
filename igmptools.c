#include <sys/types.h>  /* for type definitions */
#include <sys/socket.h> /* for socket API calls */
#include <netinet/in.h> /* for address structs */
#include <arpa/inet.h>  /* for sockaddr_in */
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() */
#include <string.h>     /* for strlen() */
#include <unistd.h>     /* for close() */
#include <netdb.h>
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
int ipv4_join_group(int fd, const char* group_addr, const char* multi_if_addr)
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



//group_addr: group address to join
//multi_if_addr: address of the interface from which the group is joined in
int ipv6_join_group(int fd, struct addrinfo* p_mc_addr)
{
    struct ipv6_mreq   mreq;
    int ret;

    memset(&mreq, 0, sizeof(mreq));
    memcpy(&mreq.ipv6mr_multiaddr,
        &((struct sockaddr_in6*)(p_mc_addr->ai_addr))->sin6_addr,
        sizeof(mreq.ipv6mr_multiaddr));
    /* Accept multicast from any interface */
    mreq.ipv6mr_interface = 0;

    ret = setsockopt(fd, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
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

    fprintf(stderr, "%s join -h -i <InterfaceName> -g <GroupIP> -p <port> -t <exit_in_seconds>\n",cmd);
    fprintf(stderr, "%s leave -h -i <InterfaceName> -g <GroupIP> -p <port>\n",cmd);
    fprintf(stderr, "%s joinleave -h -i <InterfaceName>  -g <GroupIP> -p <port> -c <counter>\n",cmd);
    return;
}

int main(int argc, char *argv[]) 
{
    char group_addr_str[32] = {0};   /* multicast IP address */
    char tmp_str[32] = {0};   /* multicast IP address */
    char if_addr_str[32] = {0};
    char *p_group_addr = "224.1.2.3";   /* multicast IP address */
    char *if_name = NULL;
    char *p_multi_if_addr;
    char * mc_port = "1234";       /* multicast port */
    unsigned short counter;
    int i;
    int fd;
    char *p;
    int timeout_in_seconds = 30;
    struct addrinfo*  multicastAddr;            /* Multicast Address */
    struct addrinfo*  localAddr;                /* Local address to bind to */
    struct addrinfo   hints          = { 0 };   /* Hints for name lookup */
    
    struct in_addr addr;
    struct in_addr net;

    /* validate the arguments */
    if (argc < 3 || 
        (strcmp(argv[1], "join") && 
        strcmp(argv[1], "leave") && 
        strcmp(argv[1], "joinleave"))) {
        usage(argv[0]);
        exit(1);

    }

    for ( i = 2; i < argc; i++ ) {
        if (0 == strcmp(argv[i], "-h")) {
            usage(argv[0]);
            return 0;
        }
        else if (0 == strcmp(argv[i], "-i") && (i+1 < argc)) {
            if_name = argv[i+1];
            i++;
        }
        else if (0 == strcmp(argv[i], "-p") && (i+1 < argc)) {
            mc_port = argv[i+1];
            i++;
        }
        else if (0 == strcmp(argv[i], "-g") && (i+1 < argc)) {
            p_group_addr = argv[i+1];
            i++;
        }
        else if (0 == strcmp(argv[i], "-t") && (i+1 < argc)) {
            timeout_in_seconds = atoi(argv[i+1]);
            i++;
        }
        else if (0 == strcmp(argv[i], "-c") && (i+1 < argc)) {
            counter = atoi(argv[i+1]);
            i++;
        }
        else {
            usage(argv[0]);
            return -1;
        }
    }

    ifm_get_info(if_name, &addr.s_addr, &net.s_addr, NULL);
    ifm_ipaddr(*(unsigned int*)&addr, if_addr_str);
    p_multi_if_addr = if_addr_str;
    
    if (0 == strcmp(argv[1], "join")) {
        /* Resolve the multicast group address */
        hints.ai_family = PF_UNSPEC;
        hints.ai_flags  = AI_NUMERICHOST;
        if ( getaddrinfo(p_group_addr, NULL, &hints, &multicastAddr) != 0 ) {
            fprintf(stderr, "failed to getaddrinfo()\n");
            return -1;
        }

        printf("Using %s\n", multicastAddr->ai_family == PF_INET6 ? "IPv6" : "IPv4");

        /* Get a local address with the same family (IPv4 or IPv6) as our multicast group */
        hints.ai_family   = multicastAddr->ai_family;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags    = AI_PASSIVE; /* Return an address we can bind to */
        if ( getaddrinfo(NULL, mc_port, &hints, &localAddr) != 0 ) {
            fprintf(stderr, "failed to next getaddrinfo()\n");
            return -1;
        }

        /* Create socket for receiving datagrams */
        if ( (fd = socket(localAddr->ai_family, localAddr->ai_socktype, 0)) < 0 ) {
            fprintf(stderr, "failed to create socket%s(%d)\n", strerror(errno),errno);
            return -1;
        }

         /* Bind to the multicast port */
        if ( bind(fd, localAddr->ai_addr, localAddr->ai_addrlen) != 0 ) {
            fprintf(stderr, "failed to bind socket%s(%d)\n", strerror(errno),errno);
            return -1;
        }

        if ( multicastAddr->ai_family  == PF_INET &&  
            multicastAddr->ai_addrlen == sizeof(struct sockaddr_in) ) {
            ipv4_join_group(fd, p_group_addr, p_multi_if_addr);
        }
        else if ( multicastAddr->ai_family  == PF_INET6 &&
            multicastAddr->ai_addrlen == sizeof(struct sockaddr_in6) ) {
            ipv6_join_group(fd, multicastAddr);
        }

        usleep(timeout_in_seconds * MICRO);

        close_mc_socket(fd);
        return 0;
    }
    else if (0 == strcmp(argv[1], "leave") && argc >= 4) {
        p_group_addr = argv[3];
        mc_port = argv[4];

        fd = open_mc_socket(atoi(mc_port));
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
        mc_port = argv[4];

        p = strrchr(p_group_addr, '.');
        memcpy(tmp_str, p_group_addr, p - p_group_addr);

        fd = open_mc_socket(atoi(mc_port));
        if (fd < 0) 
            return -1;

        for (i = 1; i <= counter; i++) {
            sprintf(group_addr_str, "%s.%d", tmp_str, i);

            fprintf(stderr, "ADD  : %s\n",group_addr_str);
            //join_group(fd, group_addr_str, p_multi_if_addr);
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

