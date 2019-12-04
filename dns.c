#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


static void usage()
{
    printf("dns [name]\n");
    printf("dns -r [ipaddr]\n");
    printf("EXAMPLES:\n");
    printf("dns -r 8.8.8.8\n");
    printf("dns -r 8.8.4.4\n");
    printf("dns -r 127.0.0.1\n");
    return;
}


int main(int argc, char *argv[])
{
    struct in_addr addr;
    struct hostent *he;

    if (1 == argc) {
        usage();
        return -1;
    }

    if (0 == strcmp("-r", argv[1]) && 3 == argc) {
        addr.s_addr = inet_addr(argv[2]);
        if (addr.s_addr == 0 || addr.s_addr == (unsigned int)(-1))
        {
            perror("inet_addr");
            return -1;
        }

        he = gethostbyaddr(&addr, 4, AF_INET);
        if (he == NULL)
        {
            switch (h_errno)
            {
                case HOST_NOT_FOUND:
                    printf("ERROR: HOST_NOT_FOUND\n");
                    break;
                case NO_ADDRESS:
                    printf("ERROR: NO_ADDRESS\n");
                    break;
                case NO_RECOVERY:
                    printf("ERROR: NO_RECOVERY\n");
                    break;
                case TRY_AGAIN:
                    printf("ERROR: TRY_AGAIN\n");
                    break;
            }
            return 1;
        }

        printf("name: %s\n", he->h_name);
    }
    else if(argc == 2){
        /*for (i =0; i < 3; i++)*/ {
        he = gethostbyname(argv[1]);
        if (he == NULL)
        {
            switch (h_errno)
            {
                case HOST_NOT_FOUND:
                    printf("ERROR: HOST_NOT_FOUND\n");
                    break;
                case NO_ADDRESS:
                    printf("ERROR: NO_ADDRESS\n");
                    break;
                case NO_RECOVERY:
                    printf("ERROR: NO_RECOVERY\n");
                    break;
                case TRY_AGAIN:
                    printf("ERROR: TRY_AGAIN\n");
                    break;
            }
            return -1;
        }

        while (*he->h_aliases) {
            printf("alias: %s\n", *he->h_aliases);
            he->h_aliases++;
        }
        
        while (*he->h_addr_list)
        {
            memcpy((char *) &addr, *he->h_addr_list, sizeof(addr));
            he->h_addr_list++;
            printf("IP address: %s\n", inet_ntoa(addr));
        }
        }
    }
    return 0;
}
