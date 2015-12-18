#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <linux/route.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "netutils.h"

#define TMP_BUF_SIZE 8*1024
char byte_buf[TMP_BUF_SIZE];


int main( int argc, char** argv)
{
    int ret = -1;
    int fd;
    int size;
    int nread;


    if (argc == 2) {
        fd = STDIN_FILENO;
    }
    else if (argc == 4 && 0 == strcmp(argv[2], "-f")) {
        fd = open(argv[3], O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "Unable to open file %s: %s\n", argv[3], strerror(errno));
            return -1;
        }
    }
    else {
        fprintf(stderr, "%s <interface_name> -f <filename>\n", argv[0]);
        return -1;
    }

    nread = 0;
    while (1) {
        size = read(fd, byte_buf, TMP_BUF_SIZE);
        if (size == 0) {
            ret = 0;
            break;
        }
        else if (size < 0) {
            fprintf(stderr, "faield to read file %s: %s", argv[4], strerror(errno));
            close(fd);
            return -1;
        }
        nread += size;
    }

    close(fd);
    fprintf(stderr, "nread is %d\n", nread);

    fd = create_raw_socket(argv[1], 0, NULL);

    if (fd !=-1 && send(fd, byte_buf, nread, 0) < 0 && (errno != ENOBUFS)) {
    	fprintf(stderr, "failed to send raw data:%s\n", strerror(errno));
    	return -1;
    }

    return ret;
}


