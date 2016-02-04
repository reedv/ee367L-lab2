/*
 * Reed Villanueva
 * EE367L
 */

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
#define MAXDATASIZE 20000 // max number of bytes we can get at once

void *getInputAddr(struct sockaddr *sa);
void serverInteractionLogic(int socket_filedes);
void sendingLogic(int sending_filedes, char* command);
void processGet(char* display, char* filename);
int checkForFile(char* filename);
void listeningLogic(int listening_filedes);
void execProcess(char* process_path, char* process, char* out_buffer);
void error(char *s);
void execChildLogic(char* process_path, char* process, int in_descriptor[2], int out_descriptor[2]);
void getFileContents(char* display, char* filename);


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



/* Client-to-server interaction logic
 * ------------------------------------------------------------------------------------ */


void serverInteractionLogic(int socket_filedes) {
	const char* EXIT_CMD = "quit";
	char command[MAXDATASIZE];
	while(strcmp(command, EXIT_CMD)) {
		sendingLogic(socket_filedes, command);

		if(strcmp(command, "display")!=0)
			listeningLogic(socket_filedes);
	}

	close(socket_filedes);
	close(socket_filedes);
}

void sendingLogic(int sending_filedes, char *command) {
	printf("client367>> ");
	scanf("%s", command);

	if(strcmp(command, "check")==0 || strcmp(command, "get")==0) {
		char filename[MAXDATASIZE - strlen(command)];
		scanf("%s", filename);  // catching space separated filename
		printf("**filename=%s\n", filename);
		strcat(command, filename);
	}

	// processing client-side commands
	if(strcmp(command, "display")==0){
		char display[MAXDATASIZE];
		char filename[MAXDATASIZE];
		scanf("%s", filename);
		processGet(display, filename);
	} else {
		printf("Sending %s\n", command);
		ssize_t sendStatus = send(sending_filedes, command, MAXDATASIZE-1, 0);

		if (sendStatus == ERRNUM) {
			// send message thru socket
			perror("send");
		}
	}
}

void processGet(char* display, char* filename)
{
//	printf("**processClient/get\n");
	strcpy(display, "get");  // temp. debug output


	int has_file = checkForFile(filename);
	if(has_file) {
		getFileContents(display, filename);
	} else {
		strcpy(display, "Server did NOT find: ");
		strcat(display, filename);
	}
}

int checkForFile(char* filename)
{
	/*
	 * T(n) = O(n) for ls_buffer.lenth == n
	 */
	char ls_buffer[MAXDATASIZE];
	execProcess("/bin/ls", "ls", ls_buffer);
//	printf("**processClient/processCheck: ls_buffer: %s", ls_buffer);

	// search results of ls for match to filename
	// Current implementation assumes ls_buffer words are delimited by newline char
	int has_file = 0;
	char * token_ptr;
	token_ptr = strtok(ls_buffer,"\n");
	while (token_ptr != NULL) {
//		printf ("**processClient/processCheck:searchloop\n %s\n", token_ptr);
		if (strcmp(filename, token_ptr) == 0) {
			has_file = 1;
			return has_file;
		}

		token_ptr = strtok (NULL, "\n");
	}

	return has_file;
}


void listeningLogic(int listening_filedes) {
	printf("** entering listeningLogic\n");
	char in_buffer[MAXDATASIZE];
	int numbytes = recv(listening_filedes, in_buffer, MAXDATASIZE-1, 0);

	if ((numbytes) == ERRNUM) {
		perror("client recv");
		exit(1);
	}

	in_buffer[numbytes] = '\0';
	printf("client received:\n'%s'\n", in_buffer);
	printf("** exiting listeningLogic\n");
}



/* Process logic
 * ------------------------------------------------------------------------------------ */

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


/*
 * This function is a special case of execProcess() that uses the cat command to send to contents of
 * a given file to the server's out_buffer and so requires additional arguments for the child
 * process's execl() function. Since this is the only time additional args. are needed,
 * this function was made rather than generalize the existing execProcess() function just
 * b/c of this special case.
 */
void getFileContents(char* display, char* filename)
{
	int in_descriptor[2],
	  	out_descriptor[2],
	    pid;

	const int buffer_size = MAXDATASIZE;  //current size may be too small for some outputs

	int in_pipe_val = pipe(in_descriptor);
	int out_pipe_val = pipe(out_descriptor);
	if (in_pipe_val < 0) error("pipe in: failure");
	if (out_pipe_val < 0) error("pipe out: failure");

	int isChildProcess = ((pid=fork()) == 0);
	if (isChildProcess) {
		/* This is the child process */

		const int pipe_read = 0,
				  pipe_write = 1;

		/* Close stdin, stdout, stderr */
		int stdin = 0, stdout = 1, stderr = 2;
		close(stdin);
		close(stdout);
		close(stderr);

		dup2(in_descriptor[pipe_read], stdin);
		dup2(out_descriptor[pipe_write], stdout);
		dup2(out_descriptor[pipe_write], stderr);

		close(in_descriptor[pipe_write]);
		close(out_descriptor[pipe_read]);

		//execl("/usr/bin/hexdump", "hexdump", "-C", (char*) NULL);
		execl("/bin/cat", "cat", filename, (char*)NULL);

		error("Could not exec command");
	} else {

		/*  The following is in the parent process */

		printf("Spawned 'cat %s' as a child process at pid %d\n", filename, pid);


		const int pipe_read = 0,
				  pipe_write = 1;

		close(in_descriptor[pipe_read]);
		close(out_descriptor[pipe_write]);
		close(in_descriptor[pipe_write]);

		/* Read back any output */
		int n = read(out_descriptor[pipe_read], display, buffer_size-5);
		display[n] = 0;

		printf("This was received by the child process: \n%s\n", display);

	}
}








