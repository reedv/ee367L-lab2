/*
** server.c -- a stream socket server demo
** client connects to server, which returns a message
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "3490"  // the port users will be connecting to

#define BACKLOG 10	 // how many pending connections queue will hold

void sigchld_handler(int s);
void *getInputAddr(struct sockaddr *sa); // sockaddr used by kernel to store most addresses.
void clientInteractionLogic(int listening_filedes, int new_filedes);

int main(void)
{
	struct addrinfo hints;
	int rv;

	// Prototype: void * memset (void *block, int c, size_t size)
	// Description:
	//    This function copies the value of c (converted to an unsigned char) into each of
	//    the first size bytes of the object beginning at block. It returns the value of block.
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	struct addrinfo *servinfo;
	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and bind to the first we can
	struct addrinfo *ptr;
	int sock_filedes;  // listen on sock_filedes
	int yes=1;
	for(ptr = servinfo; ptr != NULL; ptr = ptr->ai_next) {
		sock_filedes = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (sock_filedes == -1) {
			perror("server: socket");
			continue;
		}

		if (setsockopt(sock_filedes, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))
				== -1) {
			perror("setsockopt");
			exit(1);
		}

		if (bind(sock_filedes, ptr->ai_addr, ptr->ai_addrlen) == -1) {
			close(sock_filedes);
			perror("server: bind");
			continue;
		}

		break;
	}
	if (ptr == NULL)  {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sock_filedes, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	struct sigaction sa;
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	socklen_t sin_size;
	struct sockaddr_storage their_addr; // connector's address information
	int new_filedes;  //new connection on new_filedes
	char addr_as_text[INET6_ADDRSTRLEN];
	while(1) {  // main accept() loop
		sin_size = sizeof their_addr;
		new_filedes = accept(sock_filedes, (struct sockaddr *)&their_addr, &sin_size);
		if (new_filedes == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			getInputAddr((struct sockaddr *)&their_addr),
			addr_as_text, sizeof addr_as_text);
		printf("server: got connection from %s\n", addr_as_text);

		int isChild = (fork() == 0);
		if (isChild) { // this is the child process
			clientInteractionLogic(sock_filedes, new_filedes);
		}
		close(new_filedes);  // parent doesn't need this
	}

	return 0;
}

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *getInputAddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void clientInteractionLogic(int listening_filedes, int new_filedes) {
	// this is the child process

	close(listening_filedes); // child doesn't need the listener
	if (send(new_filedes, "Hello, World!", 13, 0) == -1)
		// send message thru socket
		perror("send");

	close(new_filedes);
	exit(0);
}
