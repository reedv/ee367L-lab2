/*
 *This is an example program that was copied from the web site
 *    www.stackflow.com.  It shows how to call a program from within
 *    a C program using "execl", which is a variation of exec.
 * Also, see the answer by user smink on the same page.
 */

/*
 * This program appears to create a forked child process and allows the parent
 * to send a message to the child for processing.
 */

#include <sys/types.h> 
#include <unistd.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
 
void error(char *s); 
void execChildLogic(int in_descriptor[2], int out_descriptor[2]);

char *data = "Some input data\n";


int main()
{ 
	int in_descriptor[2],
	  	out_descriptor[2],
	    pid;

	const int buffer_size = 255;
	char buffer[buffer_size];

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
		execChildLogic(in_descriptor, out_descriptor);
	} else {

		/*  The following is in the parent process */

		printf("Spawned 'hexdump -C' as a child process at pid %d\n", pid);

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
		printf("String sent to child: %s\n\n", data);


		/* From the parent, write some data to the child's input.
		* The child should be 'hexdump', which will process this
		* data.
		*/
		write(in_descriptor[pipe_write], data, strlen(data));

		/* Because of the small amount of data, the child may block unless we
		* close its input stream. This sends an EOF to the child on it's
		* stdin.
		*/
		close(in_descriptor[pipe_write]);

		/* Read back any output */
		int n = read(out_descriptor[pipe_read], buffer, buffer_size-5);
		buffer[n] = 0;
		//  printf("-> %s",buf);  Galen replaced this line with the following
		printf("This was received by the child: %s", buffer);

		exit(0);
	}
}
 
void error(char *s) 
{ 
	perror(s);
	exit(1);
}


void execChildLogic(int in_descriptor[2], int out_descriptor[2]) {
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
	execl("/usr/bin/hexdump", "hexdump", "-C", (char*) NULL);

	/* If hexdump wasn't executed then we would still have the following
	 * function, which would indicate an error
	 */
	error("Could not exec hexdump");
}
 
