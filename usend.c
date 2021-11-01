#include <stdio.h>
#include <string.h>
#include <strings.h>
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


static char sender_buf[DATA_BUF_SIZE];

void usage()
{
    printf("Send data to Peer by UDP.\n");
    printf("USAGE:\n");
    printf("  -r <receiverIP:port>\n");
    printf("  -t <time | #>\n");
    printf("  -b <bandwidth | #[KM]>");
    printf("  -l <length | #>\n");
    printf("  -f <path of file to read>\n");
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


int send_data(int sock_fd, char *data, int data_len)
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
        ret = write(sock_fd, data, data_len);
        if (ret < 0) {
            if (errno == EINTR) {
                printf("write failed with EINTR\n");
                goto writeagain;
            } else {
                perror("call to write!");
                return -5;
            }
        }
    }
    return 0;
}

//usend -r receiverIP:port -t time -b #[KM] -l #[KM]    
int main(int argc, char *argv[])
{
    struct sockaddr_in pin;
    int sock_fd;
    int i;
    unsigned int count = 0xFFFFFFFF;
    int maxFd;
    int res;
    char receiver_ip[128];
    short receiver_port = DEFAULT_RECEIEVER_PORT;
    int time_in_seconds;
    int bandwidth_in_BytePS;
    int udp_data_len = 1360;
    double packets_per_second = 0;
    char *colon;
    int base = 1;
    unsigned int bw;
    double freq;
    char *filepath = NULL;
    int file_fd = -1;

    fd_set wset;
    FD_ZERO(&wset);

    if (argc < 3) {
        usage();
        return -1;
    }

    for ( i = 0; i < argc; i++ ) {
        if (0 == strcmp(argv[i], "-r")) {
            strncpy(receiver_ip, argv[i+1], sizeof(receiver_ip) / sizeof(receiver_ip[0]));
            colon = strchr(receiver_ip, ':');
            if (colon != NULL) {
                *colon = '\0';
                receiver_port = atoi(colon + 1);
            }
        }
        else if (0 == strcmp(argv[i], "-t")) {
            time_in_seconds = atoi(argv[i+1]);
        }
        else if (0 == strcmp(argv[i], "-f")) {
            filepath = argv[i+1];
            file_fd = open(filepath, O_RDWR);
            if (file_fd < 0) {
                printf("Unable to open file %s: %s\n", filepath, strerror(errno));
                return -1;
            }
        }
        else if (0 == strcmp(argv[i], "-c")) {
            count = atoi(argv[i+1]);
        }
        else if (0 == strcmp(argv[i], "-l")) {
            udp_data_len = atoi(argv[i+1]);
        }
        else if (0 == strcmp(argv[i], "-b")) {
            char *p;
            if ((p = strchr(argv[i+1], 'M')) != NULL|| 
                (p = strchr(argv[i+1], 'm')) != NULL) {
                base = 1024* 1024;
            }
            else if ((p = strchr(argv[i+1], 'K')) != NULL|| 
                (p = strchr(argv[i+1], 'k')) != NULL){
                    base = 1024;
            }
            printf("XXX: %s" NEWLINE, argv[i+1]);
            if (base != 1)
                *p = '\0';
            printf("XXX: %s" NEWLINE, argv[i+1]);
            bandwidth_in_BytePS = atoi(argv[i+1]) * base / 8;
        }
    }
    
    bw = bandwidth_in_BytePS * 8;
    printf("bandwidth: %dM %dK %d (bps)" NEWLINE, 
        (bw & (((1<<12) - 1)<<20))>>20,
        (bw & (((1<<10) - 1)<<10))>>10,
         bw & ((1<<10) - 1));

    packets_per_second = (bandwidth_in_BytePS*1.0) / (udp_data_len*1.0);
    freq = 1 / packets_per_second;
    printf("payload length:%d, packets_per_second: %f, freq:%f" NEWLINE, 
        udp_data_len, packets_per_second, freq);

    printf("Send one packet every %f usecond" NEWLINE, 
        freq * 1000 * 1000);
    
    printf("try to send to %s:%d" NEWLINE, receiver_ip, receiver_port);
    bzero(&pin, sizeof(pin));
    pin.sin_family = AF_INET;
    inet_pton(AF_INET, receiver_ip, &pin.sin_addr);
    pin.sin_port = htons(receiver_port);
    
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
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
        printf("sender(%s:%d) connected to receiver(%s:%d)\n", guest_ip, ntohs(guest.sin_port), serv_ip, ntohs(serv.sin_port));
    }

    struct timeval tv;
    tv.tv_sec = 0;
    int nread = 0;

    for (i = 0; i < count; i++) {
        tv.tv_usec = (int) (freq * 1000 * 1000);
        select(0, NULL, NULL, NULL, &tv);
        if (file_fd >= 0) {
    		nread = read(file_fd, sender_buf, udp_data_len);
            if (nread == 0) {
                printf("read end of file !\n");
                break;
            }
            else if (nread < 0) {
                if (errno == EINTR) {
                    printf("INTER when reading. continue.\n");
                    continue;
                }
                else {
                    printf("error when reading.\n");
                    break;
                }
            }
        }

        res = send_data(sock_fd, sender_buf, nread);
        if (res != 0)
            break;
    }
    res = send_data(sock_fd, sender_buf, 0);

    if(file_fd >= 0)close(file_fd);
    close(sock_fd);
    
    return 0;
}

