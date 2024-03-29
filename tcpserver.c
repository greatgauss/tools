#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>

#include "tcp.h"


#define DATA_BUF_SIZE  1024 * 8

static int nwritten = 0;

static char sender_buf[DATA_BUF_SIZE];
static char receiver_buf[DATA_BUF_SIZE];


static void usage()
{
    printf("usage:\n");
    printf("server port:" NEWLINE);
    printf("\t %d: PING_PONG mode by default." NEWLINE, PORT_PING_PONG);
    printf("\t %d: S ==> C mode" NEWLINE, PORT_DATA_TO_CLIENT);
    printf("tcpserver -h" NEWLINE);
    printf("tcpserver -s [server_ip] -p [server_port] -l [datalen]" NEWLINE);

    return;
}

int tcp_server_in_ping_pong_mode
    (int sock_fd, int data_len)
{
    int ret;
    fd_set rset;
    int i;

    FD_ZERO(&rset);
    FD_SET(sock_fd, &rset);

    int res = select(sock_fd + 1, &rset, NULL, NULL, NULL);
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
readagain:
        ret = read(sock_fd,receiver_buf,MAXLINE);
        printf("read %d bytes\n",ret);

        if (-1 == ret) {
            if (errno == EINTR){
                printf("read failed with EINTR\n");
                goto readagain;
            }
            else {
                perror("call to read");
                exit(1);
            }
        } else if (0 == ret) {
            printf("the peer has closed the connection\n");
            return -4;
        }

        for (i = 0; i < ret; i++) {
            if (receiver_buf[i] == '\0')
               receiver_buf[i] = '\n'; 
        }

        printf("%s", receiver_buf);
    }

writeagain:
    ret = write(sock_fd, receiver_buf, ret);
    nwritten += ret;
    printf("write %d bytes, total bytes written: %d\n",ret, nwritten);

    if (ret < 0) {
        if (errno == EINTR) {
            printf("write failed with EINTR\n");
            goto writeagain;
        } else {
            perror("call to write!");
            return -5;
        }
    }

    return 0;
}


int tcp_server_in_S_TO_C_mode
    (int sock_fd, int data_len)
{
    int ret;
    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(sock_fd, &wset);

    int res = select(sock_fd + 1, NULL, &wset, NULL, NULL);
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
writeagain:
        ret = write(sock_fd, sender_buf, data_len);

        if (ret < 0) {
            if (errno == EINTR) {
                printf("write failed with EINTR\n");
                goto writeagain;
            } else {
                perror("write failed");
                return -5;
            }
        }
    }
    return ret;
}


int main(int argc, char *argv[])
{
    struct sockaddr_in sin;
    struct sockaddr_in pin;
    int listen_fd;
    int conn_fd;
    int address_size = sizeof(pin);
    char server_ip[128] = "0.0.0.0";
    char str[INET_ADDRSTRLEN];
    int i;
    int ret;
    int data_len;
    int port;

    data_len = 26 * 4;
    port = PORT_PING_PONG;

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
        else if (0 == strcmp(argv[i], "-l") && (i+1 < argc)) {
            data_len = atoi(argv[i+1]);
            i++;
        }
        else {
            usage();
            return -1;
        }
    }

    //for (i = 0; i < DATA_BUF_SIZE; i++) {
    //    sender_buf[i] = i % 26 + 'A';
    //}

    printf("\nFIXME!!! If SIGCHLD not handled, the child will become a zombie after client exit\n");
    signal(SIGCHLD, SIG_IGN);
    
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("call to socket");
        exit(1);
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    //sin.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, server_ip, &sin.sin_addr);
    sin.sin_port = htons(port);
    ret = bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin));
    if (ret < 0) {
        perror("call to bind");
        exit(1);
    }

    ret = listen(listen_fd, 20);
    if (ret < 0) {
        perror("call to listen");
        exit(1);
    }

    printf("Accepting connections on %s:%d...\n", server_ip, port);

    while(1) {
        conn_fd = accept(listen_fd, (struct sockaddr *)&pin, (socklen_t*)&address_size);
        printf("Request from %s:%d" NEWLINE,
             inet_ntop(AF_INET, &pin.sin_addr,str,sizeof(str)),
             ntohs(pin.sin_port));

        ret = fork();
        if (-1 == ret) {
            perror("call to fork");
            exit(1);
        }
        else if (0 == ret) {
            close(listen_fd);
            if (port == PORT_PING_PONG) {
                while(1) {
                    ret = tcp_server_in_ping_pong_mode(conn_fd, data_len);
                    if(ret != 0)
                        break;
                }
            }
            else if (port == PORT_DATA_TO_CLIENT) {
                int nsend_old = 0;
                int nsend = 0;

                while(1) {
                    #define ONE_MB (1024*1024)
                    ret = tcp_server_in_S_TO_C_mode(conn_fd, data_len);
                    if(ret < 0)
                        break;
                    nsend += ret;
                    if (nsend < ONE_MB) {
                        if (nsend - nsend_old >= ONE_MB / 10) {
                            printf("send %f KB\n", nsend / 1024.0f);
                            nsend_old = nsend;
                        }
                    }
                    if (nsend - nsend_old >= ONE_MB) {
                        printf("send %f MB\n", nsend / 1024.0f / 1024.0f);
                        nsend_old = nsend;
                    }
                }
            }

            printf("close the connection\n");
            close(conn_fd);
            exit(0);
        }
        else {
            /*
            IF dont close conn_fd in parent, 
            even if child close conn_fd and exit, FIN still not be sent by kernel
            */
            close(conn_fd);
        }
    }

    return 0;
}

