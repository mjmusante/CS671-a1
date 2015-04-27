/*
 * Author: Mark Musante
 * Class: BU MET CS671, Spring 2015
 */

#define	_SVID_SOURCE

/* the following #define is required in order to use getaddrinfo() */
#define	_POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "procdata.h"

#define	BUF_LEN 1024

int
main(int argc, char *argv[])
{
	int s, fd, len = 1;
	struct addrinfo hints;
	struct addrinfo *result;
	char buf[BUF_LEN];
	char cmd;
	int bytes;

	if (argc < 2) {
		fprintf(stderr, "Enter name/ip of host to connect to\n");
		exit(1);
	}

	if (argc < 3 ||
	    (strcmp(argv[2], "snapshot") != 0 &&
	    strcmp(argv[2], "summary") != 0)) {
		fprintf(stderr, "command must be 'snapshot' or 'summary'\n");
		exit(1);
	}

	if (strcmp(argv[2], "summary") == 0) {
		cmd = '1';
	} else {
		cmd = '2';
	}

	memset(&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
	hints.ai_protocol = 0;          /* Any protocol */
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	s = getaddrinfo(argv[1], "5678", &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo %s\n", gai_strerror(s));
		exit(1);
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		exit(1);
	}

	printf("connecting to server at %s\n", argv[1]);
	if (connect(fd, result->ai_addr, result->ai_addrlen) < 0) {
		perror("connect");
		exit(1);
	}

	bytes = write(fd, &len, sizeof (len));
	if (bytes != sizeof (len)) {
		fprintf(stderr, "expented %d got %d: ", sizeof (len), bytes);
		perror("write len");
		exit(1);
	}
	if (write(fd, &cmd, 1) != 1) {
		perror("write cmd");
		exit(1);
	}
	while ((bytes = read(fd, buf, BUF_LEN)) != 0) {
		write(1, buf, bytes);
	}
	write(1, "\n", 1);

	close(fd);

	return (0);
}
