#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#include "udp.h"

void usage()
{
    printf("usage:\n");
    printf("urecv -p <port> -f <filepath>" NEWLINE);
    return;
}


int MakeSocketNonBlocking(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }

    int res = fcntl(s, F_SETFL, flags | O_NONBLOCK);
    if (res < 0) {
        return -1;
    }

    flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }
    
    return 0;
}



int MakeSocketBlocking(int s) {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }

    int res = fcntl(s, F_SETFL, flags & (~O_NONBLOCK));
    if (res < 0) {
        return -1;
    }

    flags = fcntl(s, F_GETFL, 0);
    if (flags < 0) {
        flags = 0;
    }
    
    return 0;
}

void sigint_handler(int arg)
{
    arg =arg;
    printf("IGNORE sigint.\n");
}


static char receiver_buf[DATA_BUF_SIZE];


int read_data(int sock_fd, int file_fd, int IsShowPeerInfo)
{
    int nread, nwrite;
    fd_set rset;
    struct timeval tv;
    struct sockaddr_in sa_peer;
    int sa_len = sizeof(sa_peer);
    char peer_ip[20], guest_ip[20];

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    FD_ZERO(&rset);
    FD_SET(sock_fd, &rset);
    int res = select(sock_fd + 1, &rset, NULL, NULL, NULL);
    //printf("%s: select returns %d " NEWLINE, __FUNCTION__, res);
    if (res == 0) {
        return -1;
    }
    else if (res < 0) {
        if (errno == EINTR) {
            return -2;
        }
        printf("select failed error %d (%s)" NEWLINE, errno, strerror(errno));
        return -3;
    }

    if (FD_ISSET(sock_fd, &rset)) {
        //try to receive some data, this is a blocking call
        if ((nread = recvfrom(sock_fd, receiver_buf, DATA_BUF_SIZE, 0, 
            (struct sockaddr *) &sa_peer, &sa_len)) == -1) {
            printf("recvfrom failed error %d (%s)" NEWLINE, errno, strerror(errno));
            return -1;
        }
        if (IsShowPeerInfo) {
            inet_ntop(AF_INET, &sa_peer.sin_addr, peer_ip, sizeof(peer_ip));
            printf("Peer is %s:%d\n",  peer_ip, ntohs(sa_peer.sin_port));
        }

        if (0 == nread) {
            printf("the peer has completed sending data." NEWLINE);
            return -1;
        }
        else {
            if (file_fd >= 0){
                nwrite = write(file_fd, receiver_buf, nread);
                if(nwrite < 0) {
                    printf("write file failed, error: %s\n", strerror(errno));
                    return -1;
                } else if(nwrite != nread){
                    printf("!!!WARNING!!! write bytes not completed %d/%d\n",nwrite, nread);
                }
            }
            else
                printf("%s\n",receiver_buf);
            }
    }

    return 0;
}
    
int main(int argc, char *argv[])
{
    struct sockaddr_in sin;
    int file_fd = -1;
    int sock_fd;
    unsigned int i,count = 0xFFFFFFFF;
    int res;
    char server_ip[128];
    short receiver_port = DEFAULT_RECEIEVER_PORT;
    char* local_file_name;
    if (argc != 5 && argc != 3 && argc != 1) {
        usage();
        return -1;
    }

    for ( i = 0; i < argc; i++ ) {
        if (0 == strcmp(argv[i], "-p")) {
            receiver_port = atoi(argv[i+1]);
        }
        if (0 == strcmp(argv[i], "-f")) {
            local_file_name = argv[i+1];
            if (local_file_name) {
                file_fd = open(local_file_name,O_WRONLY | O_CREAT,0644);
                if(file_fd < 0){
                    printf("create %s failed, error:%s\n", local_file_name, strerror(errno));
                    return -1;
                }
            }
        }
    }
   
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(receiver_port);

    //bind socket to port
    if(bind(sock_fd , (struct sockaddr*)&sin, sizeof(sin) ) == -1){
        printf("bind failed with error(%d) (%s)" NEWLINE, errno, strerror(errno));
        return -1;
    }

    printf("bind on 0.0.0.0:%d" NEWLINE, receiver_port);

    res = read_data(sock_fd, file_fd, 1);
    if (res != 0) {
        goto EXIT;
    }

    for (i = 0; i < count; i++) {
        res = read_data(sock_fd, file_fd, 0);
        if (res != 0)
            break;
    }

EXIT:
    if (file_fd >= 0)
        close(file_fd);

    close(sock_fd);
    
    return 0;
}

