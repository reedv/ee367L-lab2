/*
 * Reed Villanueva
 * EE367L
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

#define ERRNUM -1
#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 100 // max number of bytes we can get at once


void sigchld_handler(int s);
void *getInputAddr(struct sockaddr *sa); // sockaddr used by kernel to store most addresses.
void clientInteractionLogic(int socket_filedes);
void sendingLogic(int sending_filedes);
void listeningLogic(int listening_filedes, char* in_buffer);
void reapDeadProcesses();

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
	int listening_filedes;  // listen on sock_filedes
	int yes=1;
	for(ptr = servinfo; ptr != NULL; ptr = ptr->ai_next) {
		listening_filedes = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (listening_filedes == -1) {
			perror("server: socket");
			continue;
		}

		int set_socket_status = setsockopt(listening_filedes, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
		if (set_socket_status
				== ERRNUM) {
			perror("setsockopt");
			exit(1);
		}

		int bind_status = bind(listening_filedes, ptr->ai_addr, ptr->ai_addrlen);
		if (bind_status == ERRNUM) {
			close(listening_filedes);
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

	int listenStatus = listen(listening_filedes, BACKLOG);
	if (listenStatus == ERRNUM) {
		perror("listen");
		exit(1);
	}

	reapDeadProcesses();

	printf("server: waiting for connections...\n");

	socklen_t sin_size;
	struct sockaddr_storage client_addr; // connector's address information
	int new_filedes;  //new connection on new_filedes
	char addr_as_text[INET6_ADDRSTRLEN];
	while(1) {  // main accept() loop: waiting for client requests
		sin_size = sizeof client_addr;
		new_filedes = accept(listening_filedes, (struct sockaddr *)&client_addr, &sin_size);
		if (new_filedes == ERRNUM) {
			perror("accept");
			continue;
		}

		inet_ntop(client_addr.ss_family,
			getInputAddr((struct sockaddr *)&client_addr),
			addr_as_text, sizeof addr_as_text);
		printf("server: got connection from %s\n", addr_as_text);

		int isChild = (fork() == 0);
		if (isChild) { // this is the child process to handle client
			close(listening_filedes);  // child doesn't need the listener
			clientInteractionLogic(new_filedes);
		}

		close(new_filedes);  // parent doesn't need this
	}

	return 0;
}

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void reapDeadProcesses() {
	struct sigaction sa;
	sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == ERRNUM) {
		perror("sigaction");
		exit(1);
	}
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
 * TODO: 1. implement ability to confirm that server has received a client command DONE
 * 		 1.2. ability to con't. loop to give commands until explicit exit DONE
 * 		 2. ability to act on a client command
 */
void clientInteractionLogic(int socket_filedes) {  // this is the child process
	const char* EXIT_CMD = "quit";
	char command[MAXDATASIZE];
	while(strcmp(command, EXIT_CMD)) {
		sendingLogic(socket_filedes);
		listeningLogic(socket_filedes, command);
	}

	close(socket_filedes);
	exit(0);
}

void sendingLogic(int sending_filedes) {
	char* simple_message = "From server: message received!";
	ssize_t sendStatus = send(sending_filedes, simple_message, strlen(simple_message), 0);
	if (sendStatus == ERRNUM) {
		// send message thru socket
		perror("send");
	}
}

void listeningLogic(int listening_filedes, char* in_buffer) {
	//char in_buffer[MAXDATASIZE];
	int numbytes = recv(listening_filedes, in_buffer, MAXDATASIZE - 1, 0);
	if ((numbytes) == ERRNUM) {
		perror("server recv");
		exit(1);
	}

	in_buffer[numbytes] = '\0';
	printf("server: received '%s'\n", in_buffer);
}
