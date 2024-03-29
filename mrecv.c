#include <sys/types.h>  /* for type definitions */
#include <sys/socket.h> /* for socket API calls */
#include <netinet/in.h> /* for address structs */
#include <arpa/inet.h>  /* for sockaddr_in */
#include <stdio.h>      /* for printf() and fprintf() */
#include <stdlib.h>     /* for atoi() */
#include <string.h>     /* for strlen() */
#include <unistd.h>     /* for close() */

#define MAX_LEN  1024 * 1024   /* maximum receive string size */
#define MIN_PORT 1024   /* minimum port allowed */
#define MAX_PORT 65535  /* maximum port allowed */

char recv_buf[MAX_LEN+1];     /* buffer to receive string */

int main(int argc, char *argv[]) 
{

    int sock;                     /* socket descriptor */
    unsigned char ttl = 5;
    //int flag_on = 1;              /* socket option flag */
    struct sockaddr_in mc_addr;   /* socket address structure */
    int recv_len;                 /* length of string received */
    struct ip_mreq mc_req;        /* multicast request structure */
    char* mc_addr_str;            /* multicast IP address */
    unsigned short mc_port;       /* multicast port */
    struct sockaddr_in from_addr; /* packet source */
    unsigned int from_len;        /* source addr length */
    int ret;
    int sum = 0;

    /* validate number of arguments */
    if (argc != 3) {
        fprintf(stderr, 
        "Usage: %s <Multicast IP> <Multicast Port>\n", argv[0]);
        exit(1);
    }

    mc_addr_str = argv[1];      /* arg 1: multicast ip address */
    mc_port = atoi(argv[2]);    /* arg 2: multicast port number */

    /* validate the port range */
    if ((mc_port < MIN_PORT) || (mc_port > MAX_PORT)) {
        fprintf(stderr, "Invalid port number argument %d.\n", mc_port);
        fprintf(stderr, "Valid range is between %d and %d.\n", MIN_PORT, MAX_PORT);
        exit(1);
    }

    /* create socket to join multicast group on */
    sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if ( sock < 0) {
        perror("socket() failed");
        exit(1);
    }

    ret = setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, 
                     &ttl, sizeof(ttl));
    if (ret < 0) {
        perror("setsockopt(IP_MULTICAST_TTL) failed");
        exit(1);
    }

#if 0
    /* set reuse port to on to allow multiple binds per host */
    ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
                     &flag_on, sizeof(flag_on));
    if (ret < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }
#endif

    /* construct a multicast address structure */
    memset(&mc_addr, 0, sizeof(mc_addr));
    mc_addr.sin_family      = AF_INET;
    mc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    mc_addr.sin_port        = htons(mc_port);

    ret = bind(sock, (struct sockaddr *) &mc_addr, sizeof(mc_addr));
    /* bind to multicast address to socket */
    if (ret < 0) {
        perror("bind() failed");
        exit(1);
    }
    
    /* construct an IGMP join request structure */
	memset(&mc_req, 0, sizeof(mc_req));
    mc_req.imr_multiaddr.s_addr = inet_addr(mc_addr_str);
    mc_req.imr_interface.s_addr = htonl(INADDR_ANY);

    /* send an ADD MEMBERSHIP message via setsockopt */
    ret=setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                   (void*) &mc_req, sizeof(mc_req));
    if (ret < 0) {
        perror("setsockopt(IP_ADD_MEMBERSHIP) failed");
        exit(1);
    }

    fprintf(stderr, "WAITING...\n");
    for (;;) {          /* loop forever */
        /* clear the receive buffers & structs */
        memset(recv_buf, 0, sizeof(recv_buf));
        from_len = sizeof(from_addr);
        memset(&from_addr, 0, from_len);

        /* block waiting to receive a packet */
        if ((recv_len = recvfrom(sock, recv_buf, MAX_LEN, 0, 
        (struct sockaddr*)&from_addr, (socklen_t *)&from_len)) < 0) {
        perror("recvfrom() failed");
        exit(1);
        }

        sum += recv_len;
        /* output received string */
        //printf("Received %d bytes from %s: ", recv_len, inet_ntoa(from_addr.sin_addr));
        printf("Received %d(%d)\n", recv_len, sum);
        //printf("%s", recv_buf);
    }

    /* send a DROP MEMBERSHIP message via setsockopt */
    ret = setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
                     (void*) &mc_req, sizeof(mc_req));
    if (ret < 0) {
        perror("setsockopt(IP_DROP_MEMBERSHIP) failed");
        exit(1);
    }

    close(sock);  
}
