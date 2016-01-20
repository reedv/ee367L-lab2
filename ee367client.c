#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

void *getInputAddr(struct sockaddr *sa);
void serverInteractionLogic(int sock_filedes);

int main(int argc, char *argv[])
{
	struct addrinfo hints, *servinfo;  // addrinfo structs

	int usage_error = (argc != 2);
	if (usage_error) {
	    fprintf(stderr,"usage: client hostname\n");
	    exit(1);
	}

	// Prototype: void * memset (void *block, int c, size_t size)
	// Description:
	//    This function copies the value of c (converted to an unsigned char) into each of
	//    the first size bytes of the object beginning at block. It returns the value of block.
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	char* hostname = argv[1];
	int rv = getaddrinfo(hostname, PORT, &hints, &servinfo);
	int get_addr_info_failed = (rv != 0);
	if (get_addr_info_failed) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	struct addrinfo *ptr;
	int sock_filedes;
	for(ptr = servinfo; ptr != NULL; ptr = ptr->ai_next) {
		sock_filedes = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		int sock_filedes_error = (sock_filedes == -1);
		if (sock_filedes_error) {
			perror("client: socket");
			continue;
		}

		int connect_sock_error = connect(sock_filedes, ptr->ai_addr, ptr->ai_addrlen)
								 == -1;
		if (connect_sock_error) {
			close(sock_filedes);
			perror("client: connect");
			continue;
		}

		break;
	}
	if (ptr == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	void* addr_as_binary = getInputAddr((struct sockaddr*) ptr->ai_addr);
	char addr_as_text[INET6_ADDRSTRLEN];
	inet_ntop(ptr->ai_family, addr_as_binary,
			  addr_as_text, sizeof addr_as_text);
	printf("client: connecting to %s\n", addr_as_text);

	freeaddrinfo(servinfo); // all done with this structure


	// Prototype: int recv (int socket, void *buffer, size_t size, int flags)
	// Description:
	// The recv function is like read, but with the additional flags flags.
	//    The possible values of flags are described in Socket Data Options.
	//    This function returns the number of bytes received, or -1 on failure.
	serverInteractionLogic(sock_filedes);
	return 0;
}

// get sockaddr, IPv4 or IPv6:
void *getInputAddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void serverInteractionLogic(int sock_filedes) {
	// Prototype: int recv (int socket, void *buffer, size_t size, int flags)
	// Description:
	// The recv function is like read, but with the additional flags flags.
	//    The possible values of flags are described in Socket Data Options.
	//    This function returns the number of bytes received, or -1 on failure.
	char in_buffer[MAXDATASIZE];
	int numbytes = recv(sock_filedes, in_buffer, MAXDATASIZE - 1, 0);
	if ((numbytes) == -1) {
		perror("recv");
		exit(1);
	}
	in_buffer[numbytes] = '\0';
	printf("client: received '%s'\n", in_buffer);
	close(sock_filedes);
}