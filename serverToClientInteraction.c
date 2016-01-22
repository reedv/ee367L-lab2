/*
 * serverToClientInteraction.c
 *
 *  Created on: Jan 22, 2016
 *      Author: reedvillanueva
 */

#include "serverToClientInteraction.h"

void clientInteractionLogic(int socket_filedes) {  // this is the child process
	const char* EXIT_CMD = "quit";
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

void listeningLogic(int listening_filedes, char* in_buffer) {
	int numbytes = recv(listening_filedes, in_buffer, MAXDATASIZE - 1, 0);
	if ((numbytes) == ERRNUM) {
		perror("server recv");
		exit(1);
	}

	in_buffer[numbytes] = '\0';
	printf("server received: %s\n", in_buffer);
}

void processClientMessage(char* command, char* out_buffer) {
	/*
	 * If other commands have not implemented logic to overwrite the server's output buffer,
	 *    client calling any any valid command after calling "list" causes the server output
	 *    to still display the list output.
	 */
	printf("**processClientMessage: command=%s\n", command);
	if(strcmp(command, "test") == 0) {
		strcpy(out_buffer, command);
	} else if(strcmp(command, "list") == 0) {  //process client command: list
		processList(out_buffer);
	} else if(strncmp(command, "check", 5) == 0) {  // processes client command: check filename
		processCheck(out_buffer);
	} else if(strncmp(command, "get", 3) == 0) {  // processes client command: get filename
		processGet(out_buffer);
	} else if(strcmp(command, "quit") == 0) {
		strcpy(out_buffer, "goodbye");
	} else {
		printf("**processClientMessage/default\n");
		strcpy(out_buffer, "**command not recognized");
	}

	printf("**exiting processClientMessage\n");
}
void processList(char* out_buffer) {
	execProcess("/bin/ls", "ls", out_buffer);
}
void processCheck(char* out_buffer) {
	printf("**processClient/check\n");
	strcpy(out_buffer, "check");  // temp. debug output

	// call ls
	// check results of ls for a match
	// put output message in out_buffer
}
void processGet(char* out_buffer) {
	printf("**processClient/get\n");
	strcpy(out_buffer, "get");  // temp. debug output

}

void sendingLogic(int sending_filedes, char* out_buffer) {
	ssize_t sendStatus = send(sending_filedes, out_buffer, strlen(out_buffer), 0);

	if (sendStatus == ERRNUM) {
		// send message thru socket
		perror("send");
	}
	printf("**exiting sendingLogic\n");
}
