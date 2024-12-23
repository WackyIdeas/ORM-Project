#include "server.h"

// Parses and deals with client commands
void* Connection(void *c)
{
	clientData* client = (clientData*)c;
	unsigned long id = client->id;
	int clientSocket = client->clientSocket;

    char clientMessage[DEFAULT_BUFLEN];
	char returnMessage[DEFAULT_BUFLEN];
	int readSize;

	printf("INFO: Client %s has connected with ID %lu\n", client->username, id);
	// Main server loop that keeps waiting for the client to send a message
    while(1)
    {
		memset(clientMessage, 0, DEFAULT_BUFLEN);
		memset(returnMessage, 0, DEFAULT_BUFLEN);
		if((readSize = recv(clientSocket, clientMessage, DEFAULT_BUFLEN, 0)) <= 0) break; // Receive message from client
        printf("DEBUG: Client %lu has sent a %d byte long message: %s\n", id, readSize, clientMessage);

		clientCommands cmd = StringToCommandClient(clientMessage);
		switch(cmd)
		{
			case LOGIN:
				HandleLogin(client, clientMessage, returnMessage);
				break;
			case USERS:
				GetUserData(client, returnMessage);
				break;
			case TALKTO:
				HandleChatRequests(client, clientMessage, returnMessage);
				break;
			case DATA:
				SendTo(client, clientMessage, returnMessage);
				break;
			case DISCONNECT:
				DisconnectChat(client, returnMessage);
				break;
			default:
				SendMessage(client, "ERROR: Unknown command");
		}
    }

    if(readSize == 0)
    {
		printf("INFO: Client %lu has disconnected\n", id);
        fflush(stdout);

		// Handle client that has been forcibly disconnected (e.g Ctrl-C)
		if(client->state == CONNECTING)
		{
			client->state = LOGGING_OUT;
			RejectChat(client, returnMessage);
		}
		else if(client->state == CHATTING)
		{
			client->state = LOGGING_OUT;
			DisconnectChat(client, returnMessage);
		}
		ClientDataRemove(client);
    }
    else if(readSize == -1)
    {
		close(clientSocket);
        perror("ERROR: recv failed");
		ClientDataRemove(client);
    }
	return 0;
}

int main(int argc , char *argv[])
{
	/*
	 * Socket boilerplate variables:
	 * socketDesc   - The file descriptor for the socket which is used by the server to
	 * 				  listen to incoming connections.
	 * clientSocket - Temporary variable used to catch the incoming client's socket
	 * 				  descriptor.
	 * c 		    - Helper variable to determine the size of sockaddr_in.
	 * server	    - Holds information about the server.
	 * client	    - Holds information about the client.
	 */
    int socketDesc, clientSocket, c;
    struct sockaddr_in server, client;
    c = sizeof(struct sockaddr_in);
	InitServer(&socketDesc, &server);

	ClientDataInit();

	// Main event loop, where we now poll for new clients instead of blocking execution while waiting for clients to connect
	while(1)
	{
   		clientSocket = accept(socketDesc, (struct sockaddr*)&client, (socklen_t*)&c);
		if (clientSocket < 0) {
			if (errno == EWOULDBLOCK) {
				// No client has connected, continue polling after a slight timeout to prevent busy waiting and high CPU usage
				nanosleep(&timeout, NULL);
				continue;
			} else {
				perror("ERROR: Error when accepting connection");
				return 1;
			}
		}
		if(clientsLen == MAX_CLIENT)
		{
			printf("WARN: Server cannot connect to any more clients!\n");
			close(clientSocket);
			continue;
		}

   		printf("INFO: Connection accepted, socket = %d\n", clientSocket);

		clientData* newClient = ClientDataAdd(clientSocket);
		if(!newClient)
		{
			printf("ERROR: Failed to create client object\n");
			break;
		}
		pthread_create(&(newClient->clientThread), NULL, Connection, (void*)(newClient));
		pthread_detach((newClient->clientThread)); // Detach the thread to prevent blocking the main thread
	}

	ClientDataDestroy();
	printf("INFO: Closing server\n");
    return 0;
}

