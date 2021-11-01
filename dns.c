#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <net/if.h>
#include <sys/un.h>

static void usage()
{
    printf("Usage:\n");
    printf("dns --gethostbyname [name]\n");
    printf("dns --gethostbyaddr [ipaddr]\n");
    printf("dns --v4 --getaddrinfo [name]\n");
    printf("dns --v6 --getaddrinfo [name]\n");
    printf("EXAMPLES:\n");
    printf("dns --gethostbyname dns.google.com\n");
    printf("dns --gethostbyaddr 8.8.8.8\n");
    printf("dns --gethostbyaddr 8.8.4.4\n");
    printf("dns --gethostbyaddr 127.0.0.1\n");
    printf("dns --getaddrinfo www.google.com\n");
    printf("dns --v4 --getaddrinfo www.google.com\n");
    printf("dns --v6 --getaddrinfo www.google.com\n");
    return;
}

char * get_dns_error_desc(int errcode)
{
    switch (errcode)
    {
        case HOST_NOT_FOUND:
            return (char*)"HOST_NOT_FOUND";
            break;
        case NO_ADDRESS:
            return (char*)"NO_ADDRESS";
            break;
        case NO_RECOVERY:
            return (char*)"NO_RECOVERY";
            break;
        case TRY_AGAIN:
            return (char*)"TRY_AGAIN";
            break;
    }

    return (char*)"UNKNOWN";
}


void x_gethostbyname(char *name)
{
    struct hostent *he;
    struct in_addr addr;

    he = gethostbyname(name);
    if (he == NULL) {
        printf("ERROR:%s(%d)\n",get_dns_error_desc(h_errno), h_errno);
        return;
    }

    while (*he->h_aliases) {
        printf("alias: %s\n", *he->h_aliases);
        he->h_aliases++;
    }

    while (*he->h_addr_list) {
        memcpy((char *) &addr, *he->h_addr_list, sizeof(addr));
        he->h_addr_list++;
        printf("IP address: %s\n", inet_ntoa(addr));
    }
}

#define ENABLE_FEATURE_UNIX_LOCAL 1
#define ENABLE_FEATURE_IPV6 1


static char*  sockaddr2str(const struct sockaddr *sa, int flags, char*buf)
{
	char host[128];
	char serv[16];
	int rc;
	socklen_t salen;

	if (ENABLE_FEATURE_UNIX_LOCAL && sa->sa_family == AF_UNIX) {
		struct sockaddr_un *sun = (struct sockaddr_un *)sa;
		sprintf(buf, "local:%.*s",
				(int) sizeof(sun->sun_path),
				sun->sun_path);
        return buf;
	}

	salen = 20;
#if ENABLE_FEATURE_IPV6
	if (sa->sa_family == AF_INET)
		salen = sizeof(struct sockaddr_in);
	if (sa->sa_family == AF_INET6)
		salen = sizeof(struct sockaddr_in6);
#endif
	rc = getnameinfo(sa, salen,
			host, sizeof(host),
	/* can do ((flags & IGNORE_PORT) ? NULL : serv) but why bother? */
			serv, sizeof(serv),
			/* do not resolve port# into service _name_ */
			flags | NI_NUMERICSERV
	);
	if (rc)
		return NULL;
	//if (flags & IGNORE_PORT)
	//	return strdup(host);
#if ENABLE_FEATURE_IPV6
	if (sa->sa_family == AF_INET6) {
		if (strchr(host, ':')) /* heh, it's not a resolved hostname */ {
			sprintf(buf, "[%s]:%s", host, serv);
            return buf;
        }
		/*return xasprintf("%s:%s", host, serv);*/
		/* - fall through instead */
	}
#endif
	/* For now we don't support anything else, so it has to be INET */
	/*if (sa->sa_family == AF_INET)*/
		sprintf(buf, "%s:%s", host, serv);
        return buf;
	/*return xstrdup(host);*/
}


#define ENABLE_FEATURE_CLEAN_UP 0
int x_getaddrinfo(const char *hostname, int querytype, const char *header)
{
	/* We can't use xhost2sockaddr() - we want to get ALL addresses,
	 * not just one */
	struct addrinfo *result = NULL;
	int rc;
	struct addrinfo hint;

	memset(&hint, 0 , sizeof(hint));
	hint.ai_family = querytype;
	hint.ai_socktype = SOCK_STREAM;
	// hint.ai_flags = AI_CANONNAME;
	rc = getaddrinfo(hostname, NULL /*service*/, &hint, &result);

	if (rc == 0) {
		struct addrinfo *cur = result;
		unsigned cnt = 0;

		printf("%-10s %s\n", header, hostname);
		// puts(cur->ai_canonname); ?
		while (cur) {
			char dotted[1024]={0}, revhost[1024]={0};
			sockaddr2str(cur->ai_addr, NI_NUMERICHOST, dotted);
			sockaddr2str(cur->ai_addr, NI_NAMEREQD, revhost);

			printf("Address %u: %s%c", ++cnt, dotted, '\n');
			cur = cur->ai_next;
		}
	} else {
		printf("can't resolve '%s': %s\n", hostname, gai_strerror(rc));
	}
	if (ENABLE_FEATURE_CLEAN_UP && result)
		freeaddrinfo(result);
	return (rc != 0);
}


int main(int argc, char *argv[])
{
    struct in_addr addr;
    struct hostent *he;
    int i;
    int querytype = AF_UNSPEC;

    if (1 == argc) {
        usage();
        return -1;
    }

    for ( i = 0; i < argc; i++ ) {
        if (0 == strcmp(argv[i], "--gethostbyname") && i+1 < argc) {
            x_gethostbyname(argv[i+1]);
            return 0;
        }

        else if (0 == strcmp(argv[i], "--gethostbyaddr") && i+1 < argc) {
            addr.s_addr = inet_addr(argv[2]);
            if (addr.s_addr == 0 || addr.s_addr == (unsigned int)(-1))
            {
                perror("inet_addr");
                return -1;
            }

            he = gethostbyaddr(&addr, 4, AF_INET);
            if (he == NULL) {
                printf("ERROR: %s(%d)\n", get_dns_error_desc(h_errno), h_errno);
                return -1;
            }

            printf("name: %s\n", he->h_name);
        }

        else if (0 == strcmp(argv[i], "--v4")) {
            querytype = AF_INET;
        }
        else if (0 == strcmp(argv[i], "--v6")) {
            querytype = AF_INET6;
        }

        else if (0 == strcmp(argv[i], "--getaddrinfo") && i+1 < argc) {
            x_getaddrinfo(argv[i+1], querytype, "Name:");
            return 0;
        }
    }
    return 0;
}
