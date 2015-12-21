#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <linux/if_arp.h>

#include "netutils.h"

void wait_for_raw_data(int fd, int timeout)
{
    fd_set readable;
    int r;
    struct timeval tv;
    struct timeval expire_at;
    struct timeval now;
    unsigned char data_buf[4096];
    int len;

    if (gettimeofday(&expire_at, NULL) < 0) {
    	printf("failed to gettimeofday");
    }
    expire_at.tv_sec += timeout;

    do {
        if (gettimeofday(&now, NULL) < 0) {
    	printf("failed to gettimeofday");
        }
        tv.tv_sec = expire_at.tv_sec - now.tv_sec;
        tv.tv_usec = expire_at.tv_usec - now.tv_usec;
        if (tv.tv_usec < 0) {
    	tv.tv_usec += 1000000;
    	if (tv.tv_sec) {
    	    tv.tv_sec--;
    	} else {
    	    /* Timed out */
    	    return;
    	}
        }
        if (tv.tv_sec <= 0 && tv.tv_usec <= 0) {
        	/* Timed out */
        	return;
        }

        FD_ZERO(&readable);
        FD_SET(fd, &readable);

        while(1) {
        	r = select(fd+1, &readable, NULL, NULL, &tv);
        	if (r >= 0 || errno != EINTR) {
                  break;
        	}
        }
        
        if (r < 0) {
        	printf("failed to select");
        }
        if (r == 0) {
        	/* Timed out */
        	return;
        }

    	/* Get the packet */
        len = recv(fd, data_buf, sizeof(data_buf), 0);
    	printf("%s: received data with len=%d\n", __FUNCTION__, len);
    } while (1);
}



#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <netdb.h>

#define GOTO(ret_val, lbl) do {ret = ret_val; goto lbl;} while(0)   
int main( int argc, char** argv)
{
    int fd;
    int type = ETH_P_ALL;

    if (1 == argc) {
        fprintf(stderr, "%s [interface_name] {ALL|ARP|RARP|IP|PPPOED|PPPOES}\n", argv[0]);        
        return -1;
    }


    if (2 == argc) {
        type = ETH_P_ALL;
    }
    else {
        char* type_str = argv[2];
        if (0 == strcasecmp("ALL", type_str))
            type = ETH_P_ALL;
        else if (0 == strcasecmp("ARP", type_str))
            type = ETH_P_ARP;
        else if (0 == strcasecmp("RARP", type_str))
            type = ETH_P_RARP;
        else if (0 == strcasecmp("IP", type_str))
            type = ETH_P_IP;
        else if (0 == strcasecmp("PPPOED", type_str))
            type = ETH_P_PPP_DISC;
        else if (0 == strcasecmp("PPPOES", type_str))
            type = ETH_P_PPP_SES;
    }

    fd = create_raw_socket( argv[1], type, NULL);
    if (fd !=-1) {
    	wait_for_raw_data(fd, 1000);
    }

    return 0;
}



