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
#include "serverToClientInteraction.h"

#define PORT "3490"  // the port users will be connecting to

#define ERRNUM -1
#define BACKLOG 10	 // how many pending connections queue will hold
#define MAXDATASIZE 255 // max number of bytes we can get at once


void sigchld_handler(int s);
void reapDeadProcesses();
void clientChildGenerator(int listening_filedes);
void *getInputAddr(struct sockaddr *sa); // sockaddr used by kernel to store most addresses.
void clientInteractionLogic(int socket_filedes);
void sendingLogic(int sending_filedes, char* out_buffer);
void listeningLogic(int listening_filedes, char* in_buffer);
void processClientMessage(char* command, char* out_buffer);
void processList(char* out_buffer);
void processCheck(char* out_buffer, char* filename);
void processGet(char* out_buffer, char* filename);
void execProcess(char* process_path, char* process, char* out_buffer);
void error(char *s);
void execChildLogic(char* process_path, char* process, int in_descriptor[2], int out_descriptor[2]);


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

	// server setup logic ----------------------------------------
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

	// client handling logic
	clientChildGenerator(listening_filedes);

	return 0;  // should this be in clientChildGnerator() ?
}

void sigchld_handler(int s)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void reapDeadProcesses()
{
	struct sigaction sa;
	sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == ERRNUM) {
		perror("sigaction");
		exit(1);
	}
}


void clientChildGenerator(int listening_filedes)
{
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
}

// get sockaddr, IPv4 or IPv6:
void *getInputAddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



void clientInteractionLogic(int socket_filedes)
{  // this is the child process
	char command[MAXDATASIZE];
	char out_buffer[MAXDATASIZE];
	while(1) {
		listeningLogic(socket_filedes, command);
		processClientMessage(command, out_buffer);
		sendingLogic(socket_filedes, out_buffer);
	}

	close(socket_filedes);
	exit(0);
}

void listeningLogic(int listening_filedes, char* in_buffer)
{
	printf("** entering listeningLogic\n");
	int numbytes = recv(listening_filedes, in_buffer, MAXDATASIZE - 1, 0);
	if ((numbytes) == ERRNUM) {
		perror("server recv");
		exit(1);
	}

	in_buffer[numbytes] = '\0';
	printf("server received: %s\n", in_buffer);
}

void processClientMessage(char* command, char* out_buffer)
{
	/*
	 * If other commands have not implemented logic to overwrite the server's output buffer,
	 *    client calling any any valid command after calling "list" causes the server output
	 *    to still display the list output.
	 */
	printf("**processClientMessage: command=%s\n", command);
	if(strcmp(command, "test") == 0) {
		strcpy(out_buffer, command);
	}
	else if(strcmp(command, "list") == 0) {  //process client command: list
		printf("**processClientMessage/processList\n");
		processList(out_buffer);
	}
	else if(strncmp(command, "check", 5) == 0) {  // processes client command: check filename
		printf("**processClientMessage/processCheck\n");
		char filename[MAXDATASIZE];
		strcpy(filename, command+strlen("check"));
		printf("**processClientMessage/processCheck: filename=%s\n", filename);

		processCheck(out_buffer, filename);
	}
	else if(strncmp(command, "get", 3) == 0) {  // processes client command: get filename
		printf("**processClientMessage/processGet\n");
		char filename[MAXDATASIZE];
		strcpy(filename, command+strlen("get"));
		printf("**processClientMessage/processCheck: filename=%s\n", filename);

		processGet(out_buffer, filename);
	}
	else if(strcmp(command, "quit") == 0) {
		strcpy(out_buffer, "goodbye");
	}
	else {
		printf("**processClientMessage/default\n");
		strcpy(out_buffer, "**command not recognized");
	}

	printf("**exiting processClientMessage\n");
}
void processList(char* out_buffer)
{
	execProcess("/bin/ls", "ls", out_buffer);
}
void processCheck(char* out_buffer, char* filename)
{
	printf("**entering processClient/processCheck\n");
	strcpy(out_buffer, "check");

	/*
	 * This was an attempt to use grep to search for the file.
	 * Using this would req. grep args. to be entered seperatly
	 * which would involve changing the signature of the execProcess()
	 * and subsequent methods to take optional params (variadic)
	 */
//	char grep_out[MAXDATASIZE];
//	grep_out[0] = 0;
//
//	char grep_search[MAXDATASIZE];
//	strcpy(grep_search, "grep . -name ");
//	strcat(grep_search, filename);
//	printf("** processClient/processCheck: grep_search: %s\n", grep_search);
//	execProcess("/usr/bin/grep", grep_search, grep_out);
//	printf("** processClient/processCheck: grep_out= %s\n", grep_out);
//
//	if(grep_out[0] == 0)
//		strcpy(out_buffer, "File not found");
//	else
//		strcpy(out_buffer, "File found");

	/*
	 * O(n) for ls_buffer.lenth == n
	 */
	char ls_buffer[MAXDATASIZE];
	execProcess("/bin/ls", "ls", ls_buffer);
	printf("**processClient/processCheck: ls_buffer: %s", ls_buffer);

	// search results of ls for match to filename
	// Current implementation assumes ls_buffer words are delimited by newline char
	int has_file = 0;
	char * token_ptr;
	token_ptr = strtok(ls_buffer,"\n");
    while (token_ptr != NULL) {
    	printf ("**processClient/processCheck:searchloop\n %s\n", token_ptr);
		if (strcmp(filename, token_ptr) == 0) {
			has_file = 1;
			break;
		}

    	token_ptr = strtok (NULL, "\n");
	}

    if(has_file) {
    	strcpy(out_buffer, "Server found: ");
    	strcat(out_buffer, filename);
    }
    else {
    	strcpy(out_buffer, "Server did not find: ");
    	strcat(out_buffer, filename);
    }

}
void processGet(char* out_buffer, char* filename)
{
	printf("**processClient/get\n");
	strcpy(out_buffer, "get");  // temp. debug output

	// check if filename exists (maybe use processCheck)
	// use cat process to put filename's contents to out_buffer

}

void sendingLogic(int sending_filedes, char* out_buffer)
{
	ssize_t sendStatus = send(sending_filedes, out_buffer, strlen(out_buffer), 0);

	if (sendStatus == ERRNUM) {
		// send message thru socket
		perror("send");
	}
	printf("**exiting sendingLogic\n");
}



/*
 * Given a process name, process, and the path to the process on the executing system, process_path,
 * the output of process is piped into the out_buffer.
 */
void execProcess(char* process_path, char* process, char* out_buffer)
{
	int in_descriptor[2],
	  	out_descriptor[2],
	    pid;

	const int buffer_size = MAXDATASIZE;  //current size may be too small for some outputs

	/* Creating two pipes: 'in' and 'out' */
	/* In a pipe, xx[0] is for reading, xx[1] is for writing */
	// The pipe function creates a pipe and puts the file descriptors for
	//    the reading and writing ends of the pipe (respectively) into
	//    filedes[0] and filedes[1].
	int in_pipe_val = pipe(in_descriptor);
	int out_pipe_val = pipe(out_descriptor);
	if (in_pipe_val < 0) error("pipe in: failure");
	if (out_pipe_val < 0) error("pipe out: failure");

	int isChildProcess = ((pid=fork()) == 0);
	if (isChildProcess) {
		/* This is the child process */
		execChildLogic(process_path, process,
				in_descriptor, out_descriptor);
	} else {

		/*  The following is in the parent process */

		printf("Spawned '%s' as a child process at pid %d\n", process, pid);

		/* This is the parent process */
		/* Close the pipe ends of the parent that the child uses to read in from / write out to so
		* the when we close the others, an EOF will be transmitted properly.
		*/

		const int pipe_read = 0,
				  pipe_write = 1;

		close(in_descriptor[pipe_read]);
		close(out_descriptor[pipe_write]);

		/* The following displays on the console what's in the array 'data'
		* The array was initialized at the top of this program with
		* the string "Some input data"
		*/
		//  printf("<- %s", data);  Galen replaced this line with the following

		//char *data = "Some input data\n";
		//printf("String sent to child: %s\n\n", data);


		/* From the parent, write some data to the child's input.
		* The child should be 'hexdump', which will process this
		* data.
		*/

		//write(in_descriptor[pipe_write], data, strlen(data));

		/* Because of the small amount of data, the child may block unless we
		* close its input stream. This sends an EOF to the child on it's
		* stdin.
		*/
		close(in_descriptor[pipe_write]);

		/* Read back any output */
		int n = read(out_descriptor[pipe_read], out_buffer, buffer_size-5);
		out_buffer[n] = 0;
		//  printf("-> %s",buf);  Galen replaced this line with the following
		printf("This was received by the child process: \n%s\n", out_buffer);

	}
}

void error(char *s)
{
	perror(s);
	exit(1);
}


void execChildLogic(char* process_path, char* process,
		int in_descriptor[2], int out_descriptor[2])
{
	/* This is the child process */

	const int pipe_read = 0,
			  pipe_write = 1;

	/* Close stdin, stdout, stderr */
	int stdin = 0, stdout = 1, stderr = 2;
	close(stdin);
	close(stdout);
	close(stderr);

	/* make our pipes, our new stdin, stdout, and stderr */
	// copies the descriptor arg1=old to descriptor number arg2=new.
	dup2(in_descriptor[pipe_read], stdin);
	dup2(out_descriptor[pipe_write], stdout);
	dup2(out_descriptor[pipe_write], stderr);

	/* Close the other ends of the pipes of the child that the parent will use, because if
	 * we leave these open in the child, the child/parent will not get an EOF
	 * when the parent/child closes their end of the pipe.
	 */
	close(in_descriptor[pipe_write]);
	close(out_descriptor[pipe_read]);

	/* Over-write the child process with the hexdump binary.
	 * The zeroth argument is the path to the program 'hexdump'
	 * The second argument is the name of the program to be run, which
	 *    is 'hexdump'.  This is a bit redundant to the zeroth argument,
	 *    but it's how it works.
	 * The third argument '-C' is one of the options of hexdump.
	 * The fourth argument is a terminating symbol -- a NULL pointer
	 *    -- to indicate the end of the arguments.
	 * In general, execl accepts an arbitrary number of arguments.
	 */
	//execl("/usr/bin/hexdump", "hexdump", "-C", (char*) NULL);
	execl(process_path, process, (char*)NULL);

	/* If hexdump wasn't executed then we would still have the following
	 * function, which would indicate an error
	 */
	error("Could not exec command");
}
