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

static void usage()
{
    printf("usage:\n");
    printf("server port:" NEWLINE);
    printf("\t %d: PING_PONG mode by default." NEWLINE, PORT_PING_PONG);
    printf("\t %d: S ==> C mode" NEWLINE, PORT_DATA_TO_CLIENT);
    printf("tcpclient -h" NEWLINE);
    printf("tcpclient -s [server_ip] -p [server_port] -c [count] --recvbufsize [size]" NEWLINE);

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
    (void)arg;
    printf("IGNORE sigint.\n");
}

#define DATA_BUF_SIZE  1024 * 8
static char sender_buf[DATA_BUF_SIZE];
static char receiver_buf[DATA_BUF_SIZE];


int tcp_client_in_ping_pong_mode(int sock_fd, int id)
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
        printf("select TIMEOUT" NEWLINE);
        return -1;
    }
    else if (res < 0) {
        if (errno == EINTR) {
            printf("select is interrupted" NEWLINE);
            return -2;
        }
        printf("select failed error %d (%s)" NEWLINE, errno, strerror(errno));
        return -3;
    }

    printf("select %d: fd[%sR,%sW]" NEWLINE, res, FD_ISSET(sock_fd, &rset) ? " ":"!",
    FD_ISSET(sock_fd, &wset) ? " ":"!");

    if (FD_ISSET(sock_fd, &wset)) {
        int len = sprintf(sender_buf, "ping-%d#", id);
        //sender_buf[len] = 0;
        printf("==>: %s\n", sender_buf);
        int nwriten = write(sock_fd, sender_buf, strlen(sender_buf));
        if (nwriten < 0) {
            printf("write failed error %d (%s)" NEWLINE, errno, strerror(errno));
            return -1;
        }
    }

    sleep(2);

    if (FD_ISSET(sock_fd, &rset)) {
        nread = read(sock_fd, receiver_buf, MAXLINE);
        if (0 == nread) {
            printf("0 byte read. The peer has closed the connection." NEWLINE);
            return -1;
        }
        else {
            receiver_buf[nread] = 0;
            printf("<==(%d): %s\n",nread, receiver_buf);
        }
    }

    return 0;
}


int set_socket_receive_buffer_size(int sock_fd, int buf_size)
{
    int ret;
    int old_size = 0;
    int new_size = 0;
    socklen_t value_len = sizeof(old_size);

    ret = getsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, (void*)&old_size, &value_len);
    if (ret < 0) {
        printf("failed to getsockopt(RECVBUF): %s\n", strerror(errno));
        return ret;
    }

    ret = setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, (void*)&buf_size, sizeof(buf_size));
    if (ret < 0) {
        printf("failed to setsockopt(RECVBUF, %d): %s\n", buf_size, strerror(errno));
        return ret;
    }

    value_len = sizeof(old_size);
    ret = getsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, (void*)&new_size, &value_len);
    if (ret < 0) {
        printf("failed to getsockopt(RECVBUF) after update: %s\n", strerror(errno));
        return ret;
    }

    printf("set socket receive buffer: %d ==> %d OK.\n", old_size, new_size);
    return 0;
}


int tcp_client_in_S_TO_C_mode(int sock_fd)
{
    int nread;

    nread = read(sock_fd, receiver_buf, MAXLINE);
    if (0 == nread) {
        printf("the peer has closed the connection." NEWLINE);
        return -1;
    }
    else if( nread < 0) {
        if (errno == EAGAIN) {
            printf("nread returns with errno EAGAIN(%s)" NEWLINE, strerror(errno));
            return 0;
        }

        printf("read() returns with errno %d (%s) " NEWLINE, errno, strerror(errno));
        return -2;
    }
    else 
        printf("read %d bytes: \n",nread);

    return 0;
}

    
int bind_and_connect(int sock_fd, char *server_ip, unsigned short port)
{
    struct sockaddr_in pin;
    int maxFd;
    int res;

    fd_set wset;
    FD_ZERO(&wset);

    memset(&pin, 0, sizeof(pin));
    pin.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &pin.sin_addr);
    pin.sin_port = htons(port);
    
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
            return -1;
        }
    }

    res = select(maxFd + 1, NULL, &wset, NULL, NULL);
    if (res == 0) {
        return -1;
    }
    else if (res < 0) {
        if (errno == EINTR) {
            return -1;
        }

        printf("select failed error %d (%s)" NEWLINE, errno, strerror(errno));
        return -1;
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
            return -1;
        }
        getsockname(sock_fd, (struct sockaddr *)&guest, &guest_len);
        inet_ntop(AF_INET, &guest.sin_addr, guest_ip, sizeof(guest_ip));
        inet_ntop(AF_INET, &serv.sin_addr, serv_ip, sizeof(serv_ip));
        printf("client(%s:%d) connected to server(%s:%d)\n", guest_ip, ntohs(guest.sin_port), serv_ip, ntohs(serv.sin_port));
    }

    return sock_fd;
}

    
int main(int argc, char *argv[])
{
    int sock_fd;
    int i,count = 0xFFFFFFFF;
    int res;
    char server_ip[128];
    int port = PORT_PING_PONG;
    int recv_buf_size = 0;

    if (argc < 3) {
        usage();
        return -1;
    }

    for ( i = 1; i < argc; i++ ) {
        if (0 == strcmp(argv[i], "-h")) {
            usage();
            return 0;
        }
        else if (0 == strcmp(argv[i], "-s") && (i+1 < argc)) {
            strncpy(server_ip, argv[i+1], sizeof(server_ip) / sizeof(server_ip[0]));
            i++;
        }
        else if (0 == strcmp(argv[i], "-p") && (i+1 < argc)) {
            port = atoi(argv[i+1]);
            i++;
        }
        else if (0 == strcmp(argv[i], "-c") && (i+1 < argc)) {
            count = atoi(argv[i+1]);
            i++;
        }
        else if (0 == strcmp(argv[i], "--recvbufsize") && (i+1 < (unsigned)argc)) {
            recv_buf_size = atoi(argv[i+1]);
            i++;
        }
        else {
            usage();
            return -1;
        }
    }

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        printf("failed to create socket: %s\n", strerror(errno));
        return -1;
    }
    if (recv_buf_size != 0)
        set_socket_receive_buffer_size(sock_fd, recv_buf_size);

    printf("try to connect %s:%d" NEWLINE, server_ip, port);
    bind_and_connect(sock_fd, server_ip, port);
    if (port == PORT_PING_PONG) {
        for (i = 0; i < count; i++) {
            res = tcp_client_in_ping_pong_mode(sock_fd, i+1);
        }
    }

    if (port == PORT_DATA_TO_CLIENT) {
            printf("PORT_DATA_TO_CLIENT count:%d" NEWLINE, count);

        if (count == 0) {
            while(1);
            //count = 0xFFFFFFFF;
            //sleep(120);
        }

        for (i = 0; i < (unsigned)count; i++) {
            res = tcp_client_in_S_TO_C_mode(sock_fd);
            if (res != 0)
                break;
        }
    }

    close(sock_fd);
    return 0;
}
