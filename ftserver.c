/*
* Name: Christopher Bugsch
* Assignment: CS372 - Program 2
* Date: 6/3/18
* Description: Program creates a "server" like system to receive requests from
	a client for directory contents or text files. If the requests are valid, the 
	server will spawn a child that connects to the client's data socket and send the information.
	Once the data has been sent, the child will be killed and the parent will continue to listen 
	for new control connections until the sigint singal is received. 

	For info on cited resourses and instructions on how to use the program see 
	README.txt
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <netdb.h> 

char longMessage[1000000];	//very long string to hold data messages from large files
char buffer[1025];		// string to hold data messages
char message[50];		// string to hold command message
char* commandArgs[4];	// array or strings to hold command arguments
char* savePtr1 = NULL;	// used for calling strtok_r in getCMD
int charCount;			// int to hold chars read and sent
int controlSocketFD;	// control socket 
int controlConnectionFD = 0;	//control socket connection
int dataSocketFD;		// data socket connection

socklen_t sizeOfClientInfo;
struct sockaddr_in serverAddress, clientAddress;

pid_t spawnPid = -5;		//child pid
int childExitMethod = -5;	//child exit method

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues

/*
* Function: startControlSocket
* Inputs: string arguments from cmd line
* Outputs: socket file descriptor int
* Description: Function attempts to open a "control" socket based
on the port number passed in. If the socket is able to be created, 
it will begin listening for 1 connection and return the socketFD
*/
int startControlSocket(char *argv[]) {
	int listenSocketFD, portNumber;
	struct sockaddr_in serverAddress;

	// Set up the address struct for this process (the server)
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(argv[1]); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process

	// Set up the socket
	listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (listenSocketFD < 0) error("ERROR opening socket");

	// Enable the socket to begin listening
	if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		error("ERROR on binding");
	listen(listenSocketFD, 1); // Flip the socket on - it can now receive 1 connection

	return listenSocketFD;
}

/*
* Function: startDataSocket
* Inputs: None. Uses the global vars from the commandArgs array
* Outputs: socket file descriptor int
* Description: Function attempts to connect to the data socket created
by the client via the info passed over from the control connection. If a 
connection is made, the socketFD is returned.
*/
int startDataSocket() {
	int socketFD, portNumber;
	struct sockaddr_in clientAddress;
	struct hostent* clientHostInfo;

	// Set up the server address struct
	memset((char*)&clientAddress, '\0', sizeof(clientAddress)); // Clear out the address struct
	portNumber = atoi(commandArgs[1]); // Get the port number, convert to an integer from a string
	clientAddress.sin_family = AF_INET; // Create a network-capable socket
	clientAddress.sin_port = htons(portNumber); // Store the port number
	clientHostInfo = gethostbyname(commandArgs[0]); // Convert the machine name into a special form of address
	if (clientHostInfo == NULL) { fprintf(stderr, "SERVER: ERROR, no such host\n"); exit(0); }
	memcpy((char*)&clientAddress.sin_addr.s_addr, (char*)clientHostInfo->h_addr, clientHostInfo->h_length); // Copy in the address

																					    // Set up the socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (socketFD < 0) error("SERVER: ERROR opening data socket");

	// Connect to client
	if (connect(socketFD, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) // Connect socket to address
		error("SERVER: ERROR connecting to client data socket");

	return socketFD;
}

/*
* Function: clearBuffer
* Inputs: Nothing
* Outputs: Nothing
* Description: Function clears both the buffer string for control messages
and the longMessage string used for sending data messages.
*/
void clearBuffer() {
	memset(buffer, '\0', sizeof(buffer));	//clear buffer
	memset(longMessage, '\0', sizeof(longMessage));	//clear buffer
}

/*
* Function: sendData
* Inputs: the socketFD for which the message should be sent to (data/control)
* Outputs: Nothing
* Description: Function sends whatever data is in the buffer string through
the socket passed in.
*/
void sendData(int socketFD) {
	// Send whatever is in buffer to client via socket passed in
	charCount = send(socketFD, buffer, strlen(buffer), 0); //
	if (charCount < 0) error("CLIENT: ERROR writing to socket");
	if (charCount < strlen(buffer)) printf("SERVER: WARNING: Not all data written to socket!\n");
}

/*
* Function: getCMD
* Inputs: Nothing
* Outputs: Nothing. Saves strings to the global array 'commandArgs'
* Description: Function recieves a 'control' message from the client that
contains the request. The format of the message should be:
clientHost-clientPort-requestType-fileName(if requested)
It then splits the message by spaces and saves each string to the commandArgs array.
*/
void getCMD() {
	char* token;
	const char* delim = " ";
	int numArgs = 0;

	// Get the message from the client and display it
	memset(message, '\0', sizeof(message));
	charCount = recv(controlConnectionFD, message, sizeof(message), 0); // Read the client's message from the socket
	if (charCount < 0) error("ERROR reading from socket");

	//get first argument
	token = strtok_r(message, delim, &savePtr1);

	while (token != NULL) {
		commandArgs[numArgs] = token;	// save the argument
		numArgs++;	//increase argument counter
		token = strtok_r(NULL, delim, &savePtr1);	//get next argument
	}
}

/*
* Function: exeListCmd
* Inputs: Nothing
* Outputs:Nothing
* Description: Function gets the current directory contents and adds them to 
the buffer string to be sent to the client. 
Code to get the directory contents came directly from the source:
https://www.geeksforgeeks.org/c-program-list-files-sub-directories-directory/
Addaption to add the contents to the buffer was made by me.
*/
void exeListCMD() {
	printf("\nList directory requested on port %s\n", commandArgs[1]);
	fflush(stdout);

	char *buffPtr = buffer;
	struct dirent *de;  // Pointer for directory entry     

	DIR *dr = opendir(".");    // opendir() returns a pointer of DIR type.   

	// opendir returns NULL if couldn't open directory  
	if (dr == NULL) {
		printf("Could not open current directory");
	}
	else {
		clearBuffer();
		//loop through and get directory contents
		while ((de = readdir(dr)) != NULL) {
			strcat(buffPtr, de->d_name);	//add file name to buffer
			strcat(buffPtr, "\n");	//add a new line to buffer

		}
		//close directory
		closedir(dr);

		//send data in buffer via data connection
		sendData(dataSocketFD);
		printf("Sending directory contents to %s:%s\n", commandArgs[0], commandArgs[1]);
		fflush(stdout);
	}
}

/*
* Function: exeFileCMD
* Inputs: Nothing
* Outputs: Nothing
* Description: Function handles a file request from client. If the file does not
exist, a message will be sent to the client via the 'control' connecton. If the file 
does exist, the function will loop through the file reading in the contents and sending
a control message of how mnay bytes of data will be sent followed by the data itself. Then
it will wait to receive a confirmation message from the client to continue on. One the entire
file has been sent, the function will send a message through the control connection that 0
more bytes will be sent. This lets the client know the server has finished sending data.
*/
void exeFileCMD() {
	printf("\nFile '%s' requested on port %s\n", commandArgs[3], commandArgs[1]);
	fflush(stdout);
	clearBuffer();

	FILE * fp;
	fp = fopen(commandArgs[3], "r");	// try to open file for reading
	if (fp == NULL) {	//if file could not be opened
		strcpy(&buffer[0], "Error: File could not be opened");
		sendData(controlConnectionFD);	//send error message to client
		close(dataSocketFD);	//close the data connection
		printf("Error: File not found\n");
		exit(0);
	}
	else {
		printf("Sending file contents to %s:%s\n", commandArgs[0], commandArgs[1]);
		fflush(stdout);

		//let client know the file can be transfered
		strcpy(&buffer[0], "good");
		sendData(controlConnectionFD);	
		clearBuffer();

		//loop through file and send data to client
		while ((fgets(longMessage, 1000000, fp)) != NULL) {
			//tell client how many bytes to read
			sprintf(buffer, "%d", strlen(longMessage));
			sendData(controlConnectionFD);	
			
			// Send data to client
			charCount = send(dataSocketFD, longMessage, strlen(longMessage), 0); //
			if (charCount < 0) error("CLIENT: ERROR writing to socket");
			if (charCount < strlen(longMessage)) {
				printf("SERVER: WARNING: Not all data written to socket!\n");
				fflush(stdout);
			}

			//wait till client confirms they received the data
			charCount = recv(controlConnectionFD, buffer, 4, MSG_WAITALL);	
			clearBuffer();
		}
		clearBuffer();		//clear the buffer
		sprintf(buffer, "%d", 0);
		sendData(controlConnectionFD);	//notify client that server is done transmitting
		fclose(fp);	// close the file
	}
}

/*
* Function: receiveMessage
* Inputs: Nothing
* Outputs: Int 1 = client is done, 0 = client is not done
* Description: Function receives a message from the client to confirm
that they are finished communicating. If the client sends the message "success" the sever knows it can 
close the data connection.
*/
int receiveMessage() {
	char message[10];	//string that holds client message
	int charsRead;		// keep track of how many chars read from message

	// Clear out the message array
	memset(message, '\0', sizeof(message));

	// Get return message from server
	charsRead = recv(controlConnectionFD, message, sizeof(message), 0); // Read data from the socket
	if (charsRead < 0) error("ERROR reading from socket");

	//check if message was an success message
	if (strcmp(message, "success") == 0) {
		printf("\nClient received data. Closing connections\n");
		fflush(stdout);
		close(dataSocketFD);	//close the data socket
		clearBuffer();
		strcpy(&buffer[0], "done");
		sendData(controlConnectionFD);	//send message to client so they can close control connection
		return 1;
	}
	//else, print the message
	printf("%s\n", message);
	return 0;
}

int main(int argc, char *argv[])
{
	// validate correct number of arguments
	if (argc != 2) { fprintf(stderr, "USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args

	//start control socket
	controlSocketFD = startControlSocket(argv);

	//confirm control socket was opened
	if (controlSocketFD < 1) {
		exit(0);
	}

	// loop forever
	while (1) {
		if (controlConnectionFD == 0) {
			printf("Waiting for a control connection...\n");
		}

		// Accept a connection, blocking if one is not available until one connects
		sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
		controlConnectionFD = accept(controlSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
		if (controlConnectionFD < 0) error("ERROR on accept");

		// Send ok message to client
		clearBuffer();
		strcpy(&buffer[0], "good");
		sendData(controlConnectionFD);

		//begin fork
		spawnPid = fork();

		//child 
		if (spawnPid == 0) {
			// get the commands from client
			getCMD();

			//print the client address
			printf("Established control connection from %s:%s\n", commandArgs[0], commandArgs[1]);
			fflush(stdout);

			//establish data connection
			dataSocketFD = -1;
			dataSocketFD = startDataSocket();
			if (dataSocketFD < 0) {
				printf("error creating data socket\n");
				fflush(stdout);
				exit(0);
			}
			//execute correct command
			if (strcmp(commandArgs[2], "-l") == 0) {
				exeListCMD();	//execute the list directory function
			}
			else if (strcmp(commandArgs[2], "-g") == 0) {
				exeFileCMD();
			}
			else {
				// Send a failed message back to the client
				charCount = send(controlConnectionFD, "Invalid command: Closing Connection", 36, 0);
				if (charCount < 0) error("ERROR writing to socket");
				close(dataSocketFD); // Close the data socket which is connected to the client
				exit(0);
			}
			//confirm client recieved data
			if (receiveMessage() == 1) {
				exit(0);	//kill child
			}
		}//end child
		//parent
		else {
			//wait for child to finish
			waitpid(spawnPid, &childExitMethod, 0);
			controlConnectionFD = 0;
		} //end parent 
	}//end while loop
	close(controlSocketFD); // Close the control socket
	return 0;
}
