#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int num_active = 0;
int status;

// Error function used for reporting issues
void error(const char *msg) {
  perror(msg);
  exit(1);
} 

// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber){
 
  // Clear out the address struct
  memset((char*) address, '\0', sizeof(*address)); 

  // The address should be network capable
  address->sin_family = AF_INET;
  // Store the port number
  address->sin_port = htons(portNumber);
  // Allow a client at any address to connect to this server
  address->sin_addr.s_addr = INADDR_ANY;
}

void decode(char * plain_buffer, char * key_buffer, char * dec_buffer)
{
	int index = 0;
	int plain_char;
	int key_char;
	int char_sum;
	char alpha[28] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
	while (index < strlen(plain_buffer))
	{
		plain_char = (plain_buffer[index] - 65);
		if (plain_char < 0)
		{
			plain_char = 26;
		}

		key_char = (key_buffer[index] - 65);
		if (key_char < 0)
		{
			key_char = 26;
		}

		char_sum = plain_char - key_char;
		
		if (char_sum < 0)
		{
			char_sum += 27;
		}
		else 
		{
			char_sum = char_sum % 27;
		}

		dec_buffer[index] = alpha[char_sum];
		index++;
	}
}

int main(int argc, char *argv[]){
	int connectionSocket, charsRead;
	char buffer[256];
	struct sockaddr_in serverAddress, clientAddress;
	socklen_t sizeOfClientInfo = sizeof(clientAddress);
	pid_t pid;

	// Check usage & args
	if (argc < 2) { 
		fprintf(stderr,"USAGE: %s port\n", argv[0]); 
		exit(1);
	} 
	
	// Create the socket that will listen for connections
	int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSocket < 0) {
		error("ERROR opening socket");
	}

	// Set up the address struct for the server socket
	setupAddressStruct(&serverAddress, atoi(argv[1]));

	// Associate the socket to the port
	if (bind(listenSocket, 
			(struct sockaddr *)&serverAddress, 
			sizeof(serverAddress)) < 0){
		error("ERROR on binding");
	}

	// Start listening for connetions. Allow up to 5 connections to queue up
	listen(listenSocket, 5); 
	
	// Accept a connection, blocking if one is not available until one connects
	while(1){
		char recv_buffer[140000];
		memset(recv_buffer, '\0', sizeof(recv_buffer));

		// Accept the connection request which creates a connection socket
		if (num_active == 0)
		{
			connectionSocket = accept(listenSocket, 
					(struct sockaddr *)&clientAddress, 
					&sizeOfClientInfo); 
			num_active = 1;
		}
		else
		{
			continue;
		}

		if (connectionSocket < 0)
		{
			error("ERROR on accept");
		}

		// start child process
		pid = fork();

		if (pid == 0)
		{
			char *save_ptr = NULL;
			char *token;
			char handshake[20];
			char plain_buffer[72000];
			char key_buffer[72000];
			char dec_buffer[72000];

			memset(plain_buffer, '\0', sizeof(plain_buffer));
			memset(key_buffer, '\0', sizeof(key_buffer));
			memset(dec_buffer, '\0', sizeof(dec_buffer));

			// receive the client's handshake message
			charsRead = recv(connectionSocket, &handshake, 20, 0);
			if (strcmp(handshake, "decrypt") != 0)
			{
				charsRead = send(connectionSocket, "handshake_error", 16, 0);
			}
			else
			{
				charsRead = send(connectionSocket, "handshake_success", 18, 0); 
			}

			// Read the client's message from the socket
			int index = 0;
			while ((charsRead = recv(connectionSocket, &recv_buffer[index], 1000, 0)) > 0)
			{
				index += charsRead;

				if (recv_buffer[index - 1] == '~')
				{
					recv_buffer[index - 1] = '\0';
					break;
				}
			}

			if (charsRead < 0){
				error("ERROR reading from socket");
			}
			// printf("SERVER: I received this from the client: \"%s\"\n", recv_buffer);

			// tokenize the message
			token = strtok_r(recv_buffer, "|", &save_ptr);
			strcpy(plain_buffer, token);

			token = strtok_r(NULL, "|", &save_ptr);
			strcpy(key_buffer, token);
			
			// decrypt the received message
			decode(plain_buffer, key_buffer, dec_buffer);

			// Send message back to the client	
			dec_buffer[strcspn(key_buffer, "\n")] = '\0'; 

			char data[72000];
			sprintf(data, "%s\n~", dec_buffer);

			charsRead = send(connectionSocket, data, strlen(data), 0); 

			if (charsRead < 0){
				error("ERROR writing to socket");
			}

			// Close the connection socket for this client
			close(connectionSocket); 
			num_active = 0;
		}
		else{
			int status;
			
			pid_t id = waitpid(pid, &status, 0);

			// close socket in parent process
			close(connectionSocket);
			num_active = 0;
		}

	}
	// Close the listening socket
	close(listenSocket); 
	return 0;
}