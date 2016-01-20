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
#define ERRNUM -1
#define MAXDATASIZE 100 // max number of bytes we can get at once 

void *getInputAddr(struct sockaddr *sa);
void serverInteractionLogic(int socket_filedes);
void sendingLogic(int sending_filedes);
void listeningLogic(int listening_filedes);

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
	int socket_filedes;
	for(ptr = servinfo; ptr != NULL; ptr = ptr->ai_next) {
		socket_filedes = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		int sock_filedes_error = (socket_filedes == ERRNUM);
		if (sock_filedes_error) {
			perror("client: socket");
			continue;
		}

		int connect_sock_error = connect(socket_filedes, ptr->ai_addr, ptr->ai_addrlen)
								 == ERRNUM;
		if (connect_sock_error) {
			close(socket_filedes);
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

	//implement listening and sending logic here


	serverInteractionLogic(socket_filedes);
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



/*
 * TODO: 1. implement ability to send simple command to server
 * 		 	- add sending file descriptor
 * 		 1.2. ability to send a user-given command
 */
void serverInteractionLogic(int socket_filedes) {

	sendingLogic(socket_filedes);
	listeningLogic(socket_filedes);

	close(socket_filedes);
	close(socket_filedes);
}

void sendingLogic(int sending_filedes) {
	char* simple_message = "From client: Hello, world!";
	ssize_t sendStatus = send(sending_filedes, simple_message, strlen(simple_message), 0);

	if (sendStatus == ERRNUM) {
		// send message thru socket
		perror("send");
	}
}

void listeningLogic(int listening_filedes) {
	// Prototype: int recv (int socket, void *buffer, size_t size, int flags)
	// Description:
	// The recv function is like read, but with the additional flags flags.
	//    The possible values of flags are described in Socket Data Options.
	//    This function returns the number of bytes received, or -1 on failure.
	char in_buffer[MAXDATASIZE];
	int numbytes = recv(listening_filedes, in_buffer, MAXDATASIZE - 1, 0);
	if ((numbytes) == ERRNUM) {
		perror("client recv");
		exit(1);
	}

	in_buffer[numbytes] = '\0';
	printf("client: received '%s'\n", in_buffer);
}
