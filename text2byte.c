#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
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
#include <dirent.h>

#include <sys/types.h>
#include <fcntl.h>


//bytes[IN|OUT]:
//bytes_length[IN]:
//hex_str[IN]:
int hex_str_to_byte_sequece(char* bytes, char* hex_str) 
{
    int hlen;
    int i;
    int is_new_byte = 1;

    int j = 0;
    char hex[3] = {0, 0, 0};
    char* stop; /* not used for any real purpose */

    hlen = strlen(hex_str);

    for (i = 0; i<hlen; i++) {
        if (hex_str[i] == '/') {
            do {
                i++;
            } while (hex_str[i] != 0x0A);
        }
        if (!isxdigit(hex_str[i]))
            continue;
        
        if (is_new_byte) {
            hex[0] = hex_str[i];
            is_new_byte = 0;
        }
        else {
            hex[1] = hex_str[i];
            is_new_byte = 1;
        }

        if (is_new_byte) {
            bytes[j] = (char)strtoul(hex, &stop, 16);
            j++;
        }
    }
    return j;
}


#define TMP_BUF_SIZE 8*1024
char tmp_buf[TMP_BUF_SIZE];
char byte_buf[TMP_BUF_SIZE/2];



int main( int argc, char** argv)
{
    int ret = -1;
    int fd;
    int size;
    int nread;
    int datalen;

    if (argc == 1) {
        fd = STDIN_FILENO;
    }
    else if (argc == 3 && 0 == strcmp(argv[1], "-f")) {
        fd = open(argv[2], O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "Unable to open file %s: %s\n", argv[2], strerror(errno));
            return -1;
        }
    }
    else {
        fprintf(stderr, "text2byte -f <filename>\n");
        return -1;
    }

    nread = 0;
    while (1) {
        size = read(fd, tmp_buf, TMP_BUF_SIZE);
        if (size == 0) {
            ret = 0;
            break;
        }
        else if (size < 0) {
            fprintf(stderr, "faield to read file: %s", strerror(errno));
            return -1;
        }
        nread += size;
    }

    datalen = hex_str_to_byte_sequece(byte_buf, tmp_buf);
    fprintf(stderr, "nread is %d\n", nread);
    fprintf(stderr, "datalen is %d\n", datalen);
    write(STDOUT_FILENO, byte_buf, datalen);

    return ret;
}

