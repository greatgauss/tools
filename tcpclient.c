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

#include "tcp.h"

void usage()
{
    printf("usage:\n");
    printf("server port %d for PING_PONG mode" NEWLINE, PING_PONG_PORT);
    printf("server port %d for DATA_FROM_SERVER_TO_CLIENT mode" NEWLINE, DATA_FROM_SERVER_TO_CLIENT_PORT);
    printf("tcpclient [server_ip] [count]" NEWLINE);
    printf("tcpclient [server_ip:server_port][count]" NEWLINE);
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

#define DATA_BUF_SIZE  1024 * 8
static char sender_buf[DATA_BUF_SIZE];
static char receiver_buf[DATA_BUF_SIZE];


int tcp_client_interact_in_ping_pong_mode
    (int sock_fd, int id)
{
    int nread;
    fd_set rset, wset;
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_SET(sock_fd, &rset);
    FD_SET(sock_fd, &wset);

    int res = select(sock_fd + 1, &rset, &wset, NULL, &tv);
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

    printf("select returns %d" NEWLINE, res);
    printf("after select, fd is %s readable" NEWLINE, FD_ISSET(sock_fd, &rset) ? "":"NOT");
    printf("after select, fd is %s writable" NEWLINE, FD_ISSET(sock_fd, &wset) ? "":"NOT");

    if (FD_ISSET(sock_fd, &rset)) {
        nread = read(sock_fd, receiver_buf, MAXLINE);
        if (0 == nread) {
            printf("0 byte read. The peer has closed the connection." NEWLINE);
            return -1;
        }
        else
            printf("read %d bytes: \n%s\n",nread, receiver_buf);
    }

    if (FD_ISSET(sock_fd, &wset)) {
        sprintf(sender_buf, "PING#%d#", id);
        int nwriten = write(sock_fd, sender_buf, strlen(sender_buf));
        if (nwriten < 0) {
            printf("write failed error %d (%s)" NEWLINE, errno, strerror(errno));
            return -1;
        }
    }

    return 0;
}


int tcp_client_interact_in_pure_recevier_mode(int sock_fd)
{
    int nread;
    fd_set rset;

    FD_ZERO(&rset);
    FD_SET(sock_fd, &rset);
    int res = select(sock_fd + 1, &rset,NULL, NULL, NULL);
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
        nread = read(sock_fd, receiver_buf, MAXLINE);
        if (0 == nread) {
            printf("the peer has closed the connection." NEWLINE);
            return -1;
        }
        else
            printf("read %d bytes: \n%s\n",nread, receiver_buf);
    }

    return 0;
}
    
int main(int argc, char *argv[])
{
    struct sockaddr_in pin;
    int sock_fd;
    unsigned int i,count;
    int maxFd;
    int res;
    char server_ip[128];
    char *colon;
    int port = -1;

    fd_set wset;
    FD_ZERO(&wset);

    if (argc < 2) {
        usage();
        return -1;
    }

    if (argc < 3) {
        count = 0xFFFFFFFF;
    }
    else {
        count = atoi(argv[2]);
    }

    strncpy(server_ip, argv[1], sizeof(server_ip) / sizeof(server_ip[0]));
    colon = strchr(server_ip, ':');
    if (colon != NULL) {
        *colon = '\0';
        port = atoi(colon + 1);
    }

    if (port == -1) {
        port = PING_PONG_PORT;
    }

    printf("try to connect %s:%d" NEWLINE, server_ip, port);
    bzero(&pin, sizeof(pin));
    pin.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &pin.sin_addr);
    pin.sin_port = htons(port);
    
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    MakeSocketNonBlocking(sock_fd);

    FD_SET(sock_fd, &wset);
    maxFd = sock_fd;
    res = connect(sock_fd, (void *)&pin, sizeof(pin));

    if (-1 == res) {
        if (errno == EISCONN) {
            printf("connect() returns with errno EISCONN(%d) (%s)" NEWLINE, errno, strerror(errno));
            exit(1);
        }
        else if (errno == EINTR) {
            printf("connect() returns with errno EINTR(%d) (%s)" NEWLINE, errno, strerror(errno));
            perror("call connect");
            exit(2);
        }
        else if (errno == EINPROGRESS) {
            printf("connect() returns with errno EINPROGRESS(%d) (%s) " NEWLINE, errno, strerror(errno));
        }
        else {
            printf("connect() returns with errno %d (%s) " NEWLINE, errno, strerror(errno));
            return 3;
        }
    }

    //MakeSocketBlocking(sock_fd);
    res = select(maxFd + 1, NULL, &wset, NULL, NULL);
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

    if (FD_ISSET(sock_fd, &wset)) {
        struct sockaddr_in serv, guest;
        socklen_t serv_len = sizeof(serv);
        socklen_t guest_len = sizeof(guest);
        char serv_ip[20], guest_ip[20];

        res = getpeername(sock_fd, (struct sockaddr *)&serv, &serv_len);
        if (res < 0) {
            if (errno == ENOTCONN) {
                printf("Connection failed with error (%d) (%s)" NEWLINE, errno, strerror(errno));
            }
            else {
                printf("Connection failed with error ENOTCONN(%d) (%s)" NEWLINE, errno, strerror(errno));
            }
            return -4;
        }
        getsockname(sock_fd, (struct sockaddr *)&guest, &guest_len);
        inet_ntop(AF_INET, &guest.sin_addr, guest_ip, sizeof(guest_ip));
        inet_ntop(AF_INET, &serv.sin_addr, serv_ip, sizeof(serv_ip));
        printf("client(%s:%d) connected to server(%s:%d)\n", guest_ip, ntohs(guest.sin_port), serv_ip, ntohs(serv.sin_port));
    }

    if (port == PING_PONG_PORT) {
        for (i = 0; i < count; i++) {
            res = tcp_client_interact_in_ping_pong_mode(sock_fd, i);
            //if (res != 0)
            //    break;
        }
    }

    if (port == DATA_FROM_SERVER_TO_CLIENT_PORT) {
        if (count == 0) {
            while (1);
        }
        for (i = 0; i < count; i++) {
            res = tcp_client_interact_in_pure_recevier_mode(sock_fd);
            if (res != 0)
                break;
        }
    }

    close(sock_fd);
    
    return 0;
}

