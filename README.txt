Christopher Bugsch
CS 372 - Program 2
6/3/18

Compile: 
	Run 'compileall' from cmd line
	This will compile the ftserver.c file and give execute permission to the ftclient file

Run: 
	ftserver: Run './ftserver [valid_port_number]'
	ftclient: Run './ftclient [server_name] [server_port_number] [request] [file_name (if requested)] [valid_port_number]

Control:
	To use these programs, first run the ftserver file with the cmd above replacing
	[valid_port_number] with the port number you'd like the program to run on. If succesful,
	you will see: 
	"Waiting on a connection..." 
	Example using flip1 server: ftserver 50123

	Now run the ftclient file on another another terminal by using the cmd above 
	replacing [server_name] and [server_port_number] with the name of the server running ftserver 
	and the port number used when starting ftserver. 
	To request the server's directory enter '-l' for request and do not enter a file_name.
	To request a file, enter '-g' for request and enter the file name with extension after.
	Finaly enter a valid port number for which the client should send and receive messages/data
	
	Example directory request: ftclient flip1 50123 -l 50124
	Example file request: ftclient flip1 50123 -g test.txt 50124

	If the connection is unsuccesful, you should see an error indictating where the error
	occured. If not, you should see a message that says "Connection established." 	

	Once ftclient has been started, both programs will attempt to handle the request. Both consoles will 
	print messages indicating whether or not the request and data transfer were succesful. 

	Upon completing the request or if any errors occured, messages will display the
	result and the ftclient will close.

	If the client requests a file with the same name as an existing file, ftclient will prompt the user 
	to overwrite the existing file. 

	If the user enters 'y', the file will be overwritten with the new file received.

	As a fail safe, any other response besides 'y' will prompt the user for a new file name. The .txt extension will
	be applied automatically so the user should only enter a new file name. 

	If a new file name is given, ftclient will recheck the new name and start the process over if 
	the new file name already exists as well. 

	Once a unique name is given, ftclient will create a new file to store the received file's data in.

	ftserver will continue to listen for new client connections until a SIGINT signal is sent using
	ctrl+C
	** Note: This should not be done while there is a current connection to a client **

Sources:
	Both files were written entirely by me with the exception of the code to handle getting directory info.
	This code was taken from:
 	https://www.geeksforgeeks.org/c-program-list-files-sub-directories-directory/

	To complete this assignment I used code from the previous assignment, Program1, as well as code used
	in my otp_dec.c file from Operating Systems. 

	The following sources were used to help understand how to use specific library functions:
		- https://docs.python.org/2/howto/sockets.html
			- For help setting up and using the socket API in python
		- https://docs.python.org/2/tutorial/errors.html
			- For help handling file open errors
		- https://docs.python.org/2/library/functions.html#raw_input
			- For help using the raw_input function to get a new file name
		- https://docs.python.org/2/library/functions.html#open
			- For help using the open function to open a file for reading/writing
		