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

#define DEFAULT_PORT    "1234"
#define DEFAULT_TIMEOUT  5

#define TYPE_JOIN               "join"
#define TYPE_LEAVE              "leave"
#define TYPE_JOIN_AND_LEAVE     "joinleave"
#define TYPE_BLOCK_SRC          "block-src"
#define TYPE_ALLOW_SRC          "allow-src"

/*socket option for join group*/

#define SO_IP_ADD_MEMBERSHIP        "ip_add_membership"
#define SO_IP_ADD_SOURCE_MEMBERSHIP "ip_add_source_membership"
#define SO_MCAST_JOIN_SOURCE_GROUP  "mcast_join_source_group"
#define SO_MCAST_BLOCK_SOURCE       "mcast_block_source"
#define SO_MCAST_UNBLOCK_SOURCE     "mcast_unblock_source"
#define SO_IP_BLOCK_SOURCE          "ip_block_source"
#define SO_IP_UNBLOCK_SOURCE        "ip_unblock_source"

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
//if_addr: address of the interface from which the group is joined in
int ipv4_join_group(int fd, const char* group_addr, const char* if_addr)
{
    struct ip_mreq   mreq;
    in_addr_t addr;
    int ret;

    fprintf(stderr, "%s: group[%s], if_addr[%s])\n", __FUNCTION__, group_addr, if_addr);

    addr = inet_addr(if_addr);
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
//if_addr: address of the interface from which the group is left from
int leave_group(int fd, const char* group_addr, const char* if_addr)
{
    struct ip_mreq   mreq;
    in_addr_t addr;
    int ret;

    fprintf(stderr, "%s: group[%s], if_addr[%s])\n", __FUNCTION__, group_addr, if_addr);

    addr = inet_addr(if_addr);
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

int ipStr2addrinfo(char *ip_str, struct addrinfo **ppaddrinfo)
{
    struct addrinfo hints = { 0, 0, 0, 0, 0, NULL, NULL, NULL }; /* Hints for name lookup */

    hints.ai_family = PF_UNSPEC;
    hints.ai_flags = AI_NUMERICHOST;
    if (getaddrinfo(ip_str, NULL, &hints, ppaddrinfo) != 0) {
        return -1;
    }
    return 0;
}


/* Get a local address with the same family (IPv4 or IPv6) as our multicast group */
int getLocalAddr(int family, char* port, struct addrinfo **ppaddrinfo)
{
    struct addrinfo hints = { 0, 0, 0, 0, 0, NULL, NULL, NULL }; /* Hints for name lookup */

    hints.ai_family = family;
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(NULL, port, &hints, ppaddrinfo) != 0) {
        return -1;
    }
    return 0;
}


void setReqGroup(struct ip_mreq *req, struct addrinfo *aiGroup)
{
    memcpy(&req->imr_multiaddr,
           &((struct sockaddr_in*)(aiGroup->ai_addr))->sin_addr,
           sizeof(req->imr_multiaddr));
}


void setReqSource(struct ip_mreq_source *req, struct addrinfo *aiSrc)
{
    memcpy(&req->imr_sourceaddr,
           &((struct sockaddr_in*)(aiSrc->ai_addr))->sin_addr,
           sizeof(req->imr_sourceaddr));
}

void setReqInterfaceAddr(struct ip_mreq *req, char *ifaddr)
{
    if (ifaddr == NULL) {
        /* Accept multicast from any interface */
        req->imr_interface.s_addr = htonl(INADDR_ANY);
    } else {
        req->imr_interface.s_addr = inet_addr(ifaddr);
    }
}

//Mulicast Source Filter
void setMSFilter(int sock, struct addrinfo *aiGroup, struct addrinfo *aiSrc, char *ifaddr, int mode)
{
    int ret;
    struct in_addr *tmp;
    struct ip_msfilter *filter = NULL;
    int                 filterlen = sizeof(struct ip_msfilter);// + sizeof(struct in_addr);
    char *filterbuf = malloc(filterlen);
    filter = (struct ip_msfilter *)filterbuf;
    setReqGroup((struct ip_mreq*)filter, aiGroup);
    setReqInterfaceAddr((struct ip_mreq*)filter, ifaddr);
    filter->imsf_fmode = mode;
    filter->imsf_numsrc    = 1;
    tmp = &((struct sockaddr_in *)aiSrc->ai_addr)->sin_addr;
    #if defined(ANDROID_PLATFORM_SDK_VERSION) && ANDROID_PLATFORM_SDK_VERSION <= 19
    filter->imsf_slist[0] = *(__u32*)tmp;
    #else
    filter->imsf_slist[0] = *tmp;
    #endif

    ret = setsockopt(sock, IPPROTO_IP, IP_MSFILTER, (char *) filter, filterlen);
    if (ret)
        printf("Failed to set IP_MSFILTER");
}


void getMSFilter(int sock, struct addrinfo *aiGroup, char *ifaddr)
{
    int ret, i;
    struct ip_msfilter *filter = NULL;

    #if defined(ANDROID_PLATFORM_SDK_VERSION) && ANDROID_PLATFORM_SDK_VERSION <= 19
    __u32 *tmp;
    #else
    struct in_addr *tmp;
    #endif

    char               buf[1500];
    int                buflen = 1500;
    filter = (struct ip_msfilter *)buf;
    setReqGroup((struct ip_mreq*)filter, aiGroup);
    setReqInterfaceAddr((struct ip_mreq*)filter, ifaddr);
    ret = getsockopt(sock, IPPROTO_IP, IP_MSFILTER, (char *) buf, &buflen);
    if (ret) {
        printf("Failed to get IP_MSFILTER\n");
        return;
    }
    printf("Get MSFILTER >>>>>>\n");
    tmp = &filter->imsf_multiaddr;
    printf("multiaddr = %s\n", inet_ntoa(*(struct in_addr *)tmp));
    tmp = &filter->imsf_interface;
    printf("interface = %s\n", inet_ntoa(*(struct in_addr *)tmp));
    printf("fmode     = %s\n", (filter->imsf_fmode == MCAST_INCLUDE ? "MCAST_INCLUDE" : "MCAST_EXCLUDE"));
    printf("numsrc    = %d\n", filter->imsf_numsrc);
    for (i = 0; i < (int)filter->imsf_numsrc; i++) {
        tmp = &filter->imsf_slist[i];
        printf("sorcelist[%d]  = %s\n", i, inet_ntoa(*(struct in_addr *)tmp));
    }
    printf("Get MSFILTER <<<<<<\n");
}

//ONLY SUPPORT IPV4 NOW
int join_source_group
(int* s, char* sockopt, char *group_addr, char *port, char* source_addr, char *ifaddr)
{

    if(!s || !group_addr || !port) {
        printf("Invalid input parameter.\n");
        return -1;
    }

    int ret = 0;
    int yes = 1;
    struct addrinfo*      aiGroup;
    struct addrinfo*      aiLocal;
    int sock = 0;

    ret = ipStr2addrinfo(group_addr, &aiGroup);
    ERR_GOTO_WITH_INFO(ret, -1, LBL_ERR, "Failed to resolve group address");

    ret = (aiGroup->ai_family == PF_INET && aiGroup->ai_addrlen == sizeof(struct sockaddr_in));
    ERR_GOTO_WITH_INFO(0 == ret, -1, LBL_ERR, "ONLY Support IPv4 now");

    ret = getLocalAddr(aiGroup->ai_family, port, &aiLocal);
    ERR_GOTO_WITH_INFO(ret, -2, LBL_ERR, "Failed to get a local address with the same family");

    /* Create socket for receiving datagrams */
    sock = socket(aiLocal->ai_family, aiLocal->ai_socktype, 0);
    ERR_GOTO_WITH_INFO(sock < 0, -3, LBL_ERR, "Failed to create socket");

    ret = setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char*)&yes,sizeof(int));
    ERR_GOTO_WITH_INFO(sock < 0, -4, LBL_CLOSE, "Failed to setsockopt(REUSEADDR)");

    ret = bind(sock, aiLocal->ai_addr, aiLocal->ai_addrlen);
    ERR_GOTO_WITH_INFO(ret, -5, LBL_CLOSE, "Failed to bind");

    /* Join Group */
    if (0 == strcmp(sockopt, SO_IP_ADD_MEMBERSHIP)) {
        struct ip_mreq req;
        setReqGroup(&req, aiGroup);
        setReqInterfaceAddr(&req, ifaddr);
        ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &req, sizeof(req));
        ERR_GOTO_WITH_INFO(ret, -4, LBL_CLOSE, "Failed to join group(IP_ADD_MEMBERSHIP)");
    }
    else if (0 == strcmp(sockopt, SO_IP_ADD_SOURCE_MEMBERSHIP) ||
             0==strcmp(sockopt, SO_IP_BLOCK_SOURCE) ||
             0==strcmp(sockopt, SO_IP_UNBLOCK_SOURCE)) {
        struct ip_mreq_source reqWithSrc;

        memset(&reqWithSrc, 0x00, sizeof(reqWithSrc));
        setReqGroup((struct ip_mreq*)&reqWithSrc, aiGroup);
        setReqInterfaceAddr((struct ip_mreq*)&reqWithSrc, ifaddr);

        if(source_addr != NULL) {
            struct addrinfo* aiSrc;
            ret = ipStr2addrinfo(source_addr, &aiSrc);
            ERR_GOTO_WITH_INFO(ret, -1, LBL_CLOSE, "Failed to resolve group source address");
            setReqSource(&reqWithSrc, aiSrc);
        }

        ret = setsockopt(sock, IPPROTO_IP, IP_ADD_SOURCE_MEMBERSHIP, (char *) &reqWithSrc, sizeof(reqWithSrc));
        ERR_GOTO_WITH_INFO(ret, -4, LBL_CLOSE, "Failed to join group for source(MCAST_JOIN_SOURCE_GROUP)");
    }
    else if (0 == strcmp(sockopt, SO_MCAST_JOIN_SOURCE_GROUP) ||
             0 == strcmp(sockopt, SO_MCAST_UNBLOCK_SOURCE) ||
             0 == strcmp(sockopt, SO_MCAST_BLOCK_SOURCE)) {
        struct group_source_req gsReq;
        memset(&gsReq, 0x00, sizeof(gsReq));

        memcpy(&gsReq.gsr_group, aiGroup->ai_addr, aiGroup->ai_addrlen);
        struct addrinfo* aiSrc;
        if(source_addr != NULL) {
            ret = ipStr2addrinfo(source_addr, &aiSrc);
            ERR_GOTO_WITH_INFO(ret, -1, LBL_CLOSE, "Failed to resolve group source address");
            memcpy(&gsReq.gsr_source, aiSrc->ai_addr, aiSrc->ai_addrlen);
        }

        int opt = MCAST_JOIN_SOURCE_GROUP;
        ret = setsockopt(sock, IPPROTO_IP, opt, (char *) &gsReq, sizeof(gsReq));
        ERR_GOTO_WITH_INFO(ret, -4, LBL_CLOSE, "Failed to join group for source");

        //getMSFilter(sock, aiGroup, ifaddr);
        //setMSFilter(sock, aiGroup, aiSrc, ifaddr, MCAST_EXCLUDE);
        //getMSFilter(sock, aiGroup, ifaddr);

        if (0==strcmp(sockopt, SO_MCAST_JOIN_SOURCE_GROUP))
            return 0;

        if (0 == strcmp(sockopt, SO_MCAST_UNBLOCK_SOURCE))
            opt = MCAST_UNBLOCK_SOURCE;
        else if (0 == strcmp(sockopt, SO_MCAST_BLOCK_SOURCE))
            opt = MCAST_BLOCK_SOURCE;

        ret = setsockopt(sock, IPPROTO_IP, opt, (char *) &gsReq, sizeof(gsReq));
        ERR_GOTO_WITH_INFO(ret, -4, LBL_CLOSE, "Failed to block/unblock group source");
    }

    freeaddrinfo(aiLocal);
    freeaddrinfo(aiGroup);

    printf("Join group %s:%s OK.\n", group_addr, port);
    *s = sock;

    return 0;
LBL_CLOSE:
    close(sock);

LBL_ERR:
    if (ret)
        printf("%s: error.ret = %d\n", __FUNCTION__, ret);
    return ret;

}

int leaveMulticast(int *sk) {
    if (close(*sk) != 0) {
        printf("Leave multicast failed\n");
        return -1;
    }
    //printf("Leave multicast OK\n");
    return 0;
}


int leave_source_group(int *sk) {
    if (close(*sk) != 0) {
        printf("Leave multicast failed\n");
        return -1;
    }
    //printf("Leave multicast OK\n");
    return 0;
}


static void usage(char * cmd)
{
    fprintf(stderr, "Join into or leave from a multicast group.\n");
    fprintf(stderr, "usage:\n");

    fprintf(stderr, "    [ %s |\n", TYPE_JOIN);
    fprintf(stderr, "      %s |\n", TYPE_LEAVE);
    fprintf(stderr, "      %s |\n", TYPE_JOIN_AND_LEAVE);
    fprintf(stderr, "      %s |\n", TYPE_BLOCK_SRC);
    fprintf(stderr, "      %s ]\n", TYPE_ALLOW_SRC);
    fprintf(stderr, "    --so [ %s |\n", SO_IP_ADD_MEMBERSHIP);
    fprintf(stderr, "           %s |\n", SO_IP_ADD_SOURCE_MEMBERSHIP);
    fprintf(stderr, "           %s |\n", SO_MCAST_JOIN_SOURCE_GROUP);
    fprintf(stderr, "           %s |\n", SO_IP_BLOCK_SOURCE);
    fprintf(stderr, "           %s ]\n", SO_IP_UNBLOCK_SOURCE);
#define IFACE_MADDR_PORT        " -i eth0 -g 224.2.3.4 -p 2345 "
#define IFACE_MADDR_PORT_SRC    " -i eth0 -g 224.2.3.4 -p 2345 -s 8.8.8.8 "
#define REPEAT_ARGS             " --repeatcounter 10 --repeatintervalbase 200 "
    fprintf(stderr, "    -h: help\n");
    fprintf(stderr, "    -i <ifname>:  join into or leave from the Group at the interface @ifname\n");
    fprintf(stderr, "    -g <GroupIP>: join into or leave from the Group @GroupIP.\n");
    fprintf(stderr, "    -p <port>:    expect the multicast data is sent to @GroupIP:@port. It is %s by default.\n", DEFAULT_PORT);
    fprintf(stderr, "    -s <source>:  Address of the source which sends Multicast data.\n");
    fprintf(stderr, "    -t <seconds>: %s exit after @seconds. It is %d by default.\n", cmd, DEFAULT_TIMEOUT);
    fprintf(stderr, "    --repeatcounter <rc>: %s repeat to do sth. @rc times.\n", cmd);
    fprintf(stderr, "    --repeatintervalbase <rib>: interval base between two operations is @rib microseconds.\n");
    fprintf(stderr, "                                interval MAYBE double every operation.\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT "--so %s\n", cmd, TYPE_JOIN, SO_IP_ADD_MEMBERSHIP);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT "--so %s\n", cmd, TYPE_JOIN, SO_IP_ADD_SOURCE_MEMBERSHIP);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT "--so %s\n", cmd, TYPE_JOIN, SO_MCAST_JOIN_SOURCE_GROUP);
    fprintf(stderr, "\n");
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s\n", cmd, TYPE_JOIN, SO_IP_ADD_SOURCE_MEMBERSHIP);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s\n", cmd, TYPE_JOIN, SO_MCAST_JOIN_SOURCE_GROUP);
    fprintf(stderr, "\n");
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s\n", cmd, TYPE_JOIN, SO_MCAST_BLOCK_SOURCE);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s\n", cmd, TYPE_JOIN, SO_MCAST_UNBLOCK_SOURCE);
    fprintf(stderr, "\n");
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s\n", cmd, TYPE_JOIN, SO_IP_BLOCK_SOURCE);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s\n", cmd, TYPE_JOIN, SO_IP_UNBLOCK_SOURCE);
    fprintf(stderr, "\n");
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT "\n", cmd, TYPE_LEAVE);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT "\n", cmd, TYPE_JOIN_AND_LEAVE);

    fprintf(stderr, "\n");
    fprintf(stderr, "    repeat to do something:\n");
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT REPEAT_ARGS "\n", cmd, TYPE_JOIN_AND_LEAVE);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT "--so %s" REPEAT_ARGS "\n", cmd, TYPE_JOIN, SO_IP_ADD_MEMBERSHIP);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT "--so %s" REPEAT_ARGS "\n", cmd, TYPE_JOIN, SO_IP_ADD_SOURCE_MEMBERSHIP);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT "--so %s" REPEAT_ARGS "\n", cmd, TYPE_JOIN, SO_MCAST_JOIN_SOURCE_GROUP);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s" REPEAT_ARGS "\n", cmd, TYPE_JOIN, SO_IP_ADD_MEMBERSHIP);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s" REPEAT_ARGS "\n", cmd, TYPE_JOIN, SO_IP_ADD_SOURCE_MEMBERSHIP);
    fprintf(stderr, "    %s %s" IFACE_MADDR_PORT_SRC "--so %s" REPEAT_ARGS "\n", cmd, TYPE_JOIN, SO_MCAST_JOIN_SOURCE_GROUP);

    return;
}


int main(int argc, char *argv[]) 
{
    char group_addr_str[32] = {0};   /* multicast IP address */
    char tmp_str[32] = {0};   /* multicast IP address */
    char if_addr_str[32] = {0};
    char *p_group_addr = NULL;   /* multicast IP address */
    char *p_source_addr = NULL;
    char *sockopt = NULL;
    char *if_name = NULL;
    char * mc_port = DEFAULT_PORT;       /* multicast port */
    int i;
    int fd;
    char *p;
    int sleep_before_exit_in_seconds = DEFAULT_TIMEOUT;
    struct addrinfo*  multicastAddr;            /* Multicast Address */
    struct addrinfo*  aiLocal;                /* Local address to bind to */
    struct addrinfo   hints          = { 0 };   /* Hints for name lookup */
    
    struct in_addr addr;
    struct in_addr net;
    int repeatcounter = 0;
    int repeatinterval_in_ms = 1000;

    /* validate the arguments */
    if (argc < 3 || 
        (strcmp(argv[1], TYPE_JOIN) &&
        strcmp(argv[1], TYPE_LEAVE) &&
        strcmp(argv[1], TYPE_JOIN_AND_LEAVE) &&
        strcmp(argv[1], "DUCK"))) {
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
        else if (0 == strcmp(argv[i], "--so") && (i+1 < argc)) {
            sockopt = argv[i+1];
            i++;
        }
        else if (0 == strcmp(argv[i], "-g") && (i+1 < argc)) {
            p_group_addr = argv[i+1];
            i++;
        }
        else if (0 == strcmp(argv[i], "-s") && (i+1 < argc)) {
            p_source_addr = argv[i+1];
            i++;
        }
        else if (0 == strcmp(argv[i], "-t") && (i+1 < argc)) {
            sleep_before_exit_in_seconds = atoi(argv[i+1]);
            i++;
        }
        else if (0 == strcmp(argv[i], "--repeatcounter") && (i+1 < argc)) {
            repeatcounter =  atoi(argv[i+1]);
            i++;
        }
        else if (0 == strcmp(argv[i], "--repeatintervalbase") && (i+1 < argc)) {
            repeatinterval_in_ms =  atoi(argv[i+1]);
            i++;
        }
        else {
            usage(argv[0]);
            return -1;
        }
    }

    ifm_get_info(if_name, &addr.s_addr, &net.s_addr, NULL);
    ifm_ipaddr(*(unsigned int*)&addr, if_addr_str);
    
    if (0 == strcmp(argv[1], "DUCK")) {
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
        if ( getaddrinfo(NULL, mc_port, &hints, &aiLocal) != 0 ) {
            fprintf(stderr, "failed to next getaddrinfo()\n");
            return -1;
        }

        /* Create socket for receiving datagrams */
        if ( (fd = socket(aiLocal->ai_family, aiLocal->ai_socktype, 0)) < 0 ) {
            fprintf(stderr, "failed to create socket%s(%d)\n", strerror(errno),errno);
            return -1;
        }

         /* Bind to the multicast port */
        if ( bind(fd, aiLocal->ai_addr, aiLocal->ai_addrlen) != 0 ) {
            fprintf(stderr, "failed to bind socket%s(%d)\n", strerror(errno),errno);
            return -1;
        }

        if ( multicastAddr->ai_family  == PF_INET &&  
            multicastAddr->ai_addrlen == sizeof(struct sockaddr_in) ) {
            ipv4_join_group(fd, p_group_addr, if_addr_str);
        }
        else if ( multicastAddr->ai_family  == PF_INET6 &&
            multicastAddr->ai_addrlen == sizeof(struct sockaddr_in6) ) {
            ipv6_join_group(fd, multicastAddr);
        }

        usleep(sleep_before_exit_in_seconds * MICRO);

        close_mc_socket(fd);
        return 0;
    }
    else if (0 == strcmp(argv[1], TYPE_JOIN)) {
        int i, sock = -1;
        join_source_group(&sock, sockopt, p_group_addr, mc_port, p_source_addr, if_addr_str);
        for (i = 0; i < repeatcounter; i++) {
            usleep(repeatinterval_in_ms * 1000);
            join_source_group(&sock, sockopt, p_group_addr, mc_port, p_source_addr, if_addr_str);
            repeatinterval_in_ms *= 2;
        }

        usleep(sleep_before_exit_in_seconds * MICRO);
        leave_source_group(&sock);
        return 0;
    }
    else if (0 == strcmp(argv[1], TYPE_LEAVE) && argc >= 4) {
        //p_group_addr = argv[3];
        //mc_port = argv[4];

        fd = open_mc_socket(atoi(mc_port));
        if (fd < 0) 
            return -1;

        leave_group(fd, p_group_addr, if_addr_str);
        close_mc_socket(fd);
        return 0;
    }
    else if (0 == strcmp(argv[1], TYPE_JOIN_AND_LEAVE) && argc >= 6) {
        if (repeatcounter > 254)
            repeatcounter = 254;

        p = strrchr(p_group_addr, '.');
        memcpy(tmp_str, p_group_addr, p - p_group_addr);

        fd = open_mc_socket(atoi(mc_port));
        if (fd < 0)
            return -1;

        for (i = 1; i <= repeatcounter; i++) {
            sprintf(group_addr_str, "%s.%d", tmp_str, i);

            fprintf(stderr, "ADD  : %s\n", group_addr_str);
            ipv4_join_group(fd, group_addr_str, if_addr_str);
            usleep(repeatinterval_in_ms * 1000);

            fprintf(stderr, "LEAVE: %s\n",group_addr_str);
            leave_group(fd, group_addr_str, if_addr_str);
            usleep(1*MICRO);

        }
        close_mc_socket(fd);
        return 0;
    }

    return 0;
}
