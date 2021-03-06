#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#define PROGNAME	"reverse-listener"
#define LISTEN_BACKLOG	1

#include "common.h"

struct listener
{
	char		*host;
	char		*service;
	uint16_t	port;
	int		family;
};

static void usage(void)
{
	fprintf(stderr, "usage: %s [options] <host> <port>\n", PROGNAME);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "\t-h         : display this and exit\n");
	fprintf(stderr, "\t-v         : display version and exit\n");
	fprintf(stderr, "\t-6         : use IPv6 socket\n");
}


static void listener_init(struct listener *listener)
{
	listener->family = AF_INET;
}


static int listener_set_port(struct listener *listener, char *port)
{
	long int	res;

	res = strtol(port, NULL, 10);

	if ( res <= 0 || res > 0xffff )
		return -1;

	listener->service = port;
	listener->port = (uint16_t) res;

	return 0;
}


static void display_tip(void)
{
	fprintf(stderr, "\nNew connection !\n");
	fprintf(stderr, "Type 'quit' to close the connection\n");
}


static int command(int fd)
{
	size_t	len;
	int	llen;
	char	buf[4096];

	display_tip();

	while ( 1 )
	{
		fputs(">> ", stdout);

		if ( fgets(buf, sizeof(buf), stdin) == NULL )
		{
			fprintf(stderr, "fgets: error occured\n");
			return -1;
		}

		if ( strcmp(buf, "quit\n") == 0 )
			return 0;

		len = strnlen(buf, sizeof(buf));

		if ( send(fd, buf, len, MSG_DONTWAIT) != len )
		{
			fprintf(stderr, "write: unable to send\n");
			return 0;
		}

		while ( (llen = recv(fd, buf, sizeof(buf), MSG_DONTWAIT)) )
		{
			if ( llen < 0 )
			{
				if ( errno == EAGAIN || errno == EWOULDBLOCK )
					break;
				fprintf(stderr, "read: unable to send\n");
				return 0;
			}

			printf("%.*s", llen, buf);
		}
	}

	return 0;
}


static void serve(const struct listener *listener)
{
	int			sockfd,
				client,
				opt = 1;
	struct sockaddr		saddr;
	struct sockaddr_in	*sin4,
				peeraddr4;
	struct sockaddr_in6	*sin6,
				peeraddr6;
	struct sockaddr		*peeraddr;
	socklen_t		len;

	if ( (sockfd = socket(listener->family, SOCK_STREAM, 0)) < 0 )
	{
		perror("socket");
		exit(errno);
	}

	if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *) &opt, sizeof(opt)) < 0 )
	{
			perror("reuseaddr");
			exit(errno);
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sa_family = listener->family;

	switch ( listener->family )
	{
			case AF_INET:
			sin4 = (struct sockaddr_in *)&saddr;
			len = sizeof(struct sockaddr_in);
			sin4->sin_port = htons(listener->port);
			sin4->sin_addr.s_addr = htonl(INADDR_ANY);
			peeraddr = (struct sockaddr *) &peeraddr4;
			break;

			case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&saddr;
			len = sizeof(struct sockaddr_in6);
			sin6->sin6_port = htons(listener->port);
			memset(sin6->sin6_addr.s6_addr, 0, 16);
			peeraddr = (struct sockaddr *) &peeraddr6;
			break;

			default:
			fprintf(stderr, "Unknown family %d\n", listener->family);
			exit(-1);
	}

	if ( bind(sockfd, &saddr, len) < 0 )
	{
		perror("bind");
		exit(errno);
	}

	if ( listen(sockfd, LISTEN_BACKLOG) < 0 )
	{
		perror("listen");
		exit(errno);
	}

	fprintf(stderr, "Waiting for connection...\n");

	if ( (client = accept(sockfd, peeraddr, &len)) < 0 )
	{
		perror("accept");
		exit(errno);
	}

	while ( command(client) )
		;

	close(client);
	close(sockfd);
}


int main(int argc, char *argv[])
{
	struct listener	listener;
	int	c;

	listener_init(&listener);

	while ( (c = getopt(argc, argv, "6hv")) != -1 )
	{
		switch ( c )
		{
			case '6':
			listener.family = AF_INET6;
			break;

			case 'h':
			usage();
			return 0;

			case 'v':
			version();
			return 0;
		}
	}

	argv += optind;
	argc -= optind;

	if ( argc != 2 )
	{
		usage();
		return -1;
	}

	listener.host = argv[0];
	if ( listener_set_port(&listener, argv[1]) )
	{
		fprintf(stderr, "Invalid port %s\n", argv[1]);
		return -1;
	}

	serve(&listener);

	return 0;
}
