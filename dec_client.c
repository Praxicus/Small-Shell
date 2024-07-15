#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()

/**
* Client code
* 1. Create a socket and connect to the server specified in the command arugments.
* 2. Prompt the user for input and send that input as a message to the server.
* 3. Print the message received from the server and exit the program.
*/

// Error function used for reporting issues
void error(const char *msg) { 
	perror(msg); 
	exit(1); 
} 

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber, 
                        char* hostname){
 
	// Clear out the address struct
	memset((char*) address, '\0', sizeof(*address)); 

	// The address should be network capable
	address->sin_family = AF_INET;
	// Store the port number
	address->sin_port = htons(portNumber);

	// Get the DNS entry for this host name
	struct hostent* hostInfo = gethostbyname(hostname); 
	if (hostInfo == NULL) { 
		fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
		exit(1); 
	}
	// Copy the first IP address from the DNS entry to sin_addr.s_addr
	memcpy((char*) &address->sin_addr.s_addr, 
			hostInfo->h_addr_list[0],
			hostInfo->h_length);
}

void validateFile(char * buffer)
{
	int i = 0;
	char alpha[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

	// look for invalid characters in the text buffer
	while (i < sizeof(buffer))
	{
		if (buffer[i] == '\0' || buffer[i] == '\n')
		{
			break;
		}
		else if (strchr(alpha, buffer[i]) == NULL)
		{
			fprintf(stderr, "CLIENT: ERROR, invalid characters detected\n");
			exit(1); 
		}
		i++;
	}
}

void loadFiles(FILE *plain_file, FILE *key_file, char *plain_buffer, char *key_buffer)
{
	ssize_t buffer_size = 72000;
	char * last_char;

	// load text from provided files
	getline(&plain_buffer, &buffer_size, plain_file);
	getline(&key_buffer, &buffer_size, key_file);

	if ((last_char = strstr(plain_buffer, "\n")) != NULL) 
	{
			*last_char = '\0';
	}

	if ((last_char = strstr(key_buffer, "\n")) != NULL) 
	{
		*last_char = '\0';
	}	

	fclose(plain_file);
	fclose(key_file);

}

void handshake(int socket)
{
	char status[20];

	// send handshake message
	send(socket, "decrypt", 8, 0);

	// wait for response
	recv(socket, &status, 20, 0);

	// if handshake is bad, stop the client
	if (strcmp(status, "handshake_error") == 0)
	{
		fprintf(stderr, "CLIENT: ERROR, invalid client/server connection\n");
		exit(1); 
	}
}

int main(int argc, char *argv[]) {
	int socketFD, portNumber, charsWritten, charsRead;
	struct sockaddr_in serverAddress;

	char plain_buffer[72000];
	char key_buffer[72000];
	char dec_buffer[72000];

	memset(plain_buffer, '\0', sizeof(plain_buffer));
	memset(key_buffer, '\0', sizeof(key_buffer));
	memset(dec_buffer, '\0', sizeof(dec_buffer));

	// Check usage & args
	if (argc < 4) { 
		fprintf(stderr,"USAGE: %s plaintext key port\n", argv[0]); 
		exit(1); 
	} 

	FILE *plain_file = fopen(argv[1], "r");
	FILE *key_file = fopen(argv[2], "r");

	if (plain_file == NULL || key_file == NULL)
	{
		fprintf(stderr, "CLIENT: ERROR, no such file\n");
		exit(1);
	}

	// load data from files into buffers
	loadFiles(plain_file, key_file, plain_buffer, key_buffer);
	
	// validate the plaintext file only contains correct characters
	validateFile(plain_buffer);

	// validate key is at least as long as plaintext
	if (strlen(plain_buffer) > strlen(key_buffer))
	{
		fprintf(stderr, "CLIENT: ERROR, key too short\n");
		exit(1);
	}

	// Create a socket
	socketFD = socket(AF_INET, SOCK_STREAM, 0); 
	if (socketFD < 0){
		error("CLIENT: ERROR opening socket");
	}

	// Set up the server address struct
	setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");

	// Connect to server
	if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
		error("CLIENT: ERROR connecting");
	}
	
	handshake(socketFD);

	char data[144000];

	// cut off trailing newline character
	plain_buffer[strcspn(plain_buffer, "\n")] = '\0'; 
	key_buffer[strcspn(key_buffer, "\n")] = '\0'; 

	sprintf(data, "%s|%s~\0", plain_buffer, key_buffer);

	// Send message to server
	// Write to the server
	charsWritten = send(socketFD, data, strlen(data), 0); 
	if (charsWritten < 0){
		error("CLIENT: ERROR writing to socket");
	}

	if (charsWritten < strlen(data)){
		printf("CLIENT: WARNING: Not all data written to socket!\n");
	}

	// Get return message from server
	// Read data from the socket, leaving \0 at end
	int index = 0;
	while ((charsRead = recv(socketFD, &dec_buffer[index], 1000, 0)) > 0)
			{
				index += charsRead;

				if (dec_buffer[index - 1] == '~')
				{
					dec_buffer[index - 1] = '\0';
					break;
				}
			}

	if (charsRead < 0){
		error("CLIENT: ERROR reading from socket");
	}
	printf("%s", dec_buffer);

	// Close the socket
	close(socketFD); 
	return 0;
}