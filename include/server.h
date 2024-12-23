#ifndef SERVER_H
#define SERVER_H
#include "shared.h"
#include "map.h"

int SendMessage(clientData *client, char *message)
{
    if(send(client->clientSocket, message, strlen(message), 0) < 0)
    {
        perror("ERROR: send failed");
        close(client->clientSocket);
        ClientDataRemove(client);
        return 0;
    }
    return 1;
}
void InitServer(int *socketDesc, struct sockaddr_in *server)
{
    // Create listening socket descriptor, this is where the server listens for incoming client connections
    *socketDesc = socket(AF_INET, SOCK_STREAM, 0);
    if (*socketDesc == -1)
    {
        printf("ERROR: Could not create socket\n");
        exit(1);
    }
    printf("INFO: Socket created\n");

    /*
     * This function sets the listening socket file descriptor as a non-blocking socket.
     * This allows us to call the accept() function without blocking the main thread,
     * which then lets us poll for incoming connections.
     * https://www.man7.org/linux/man-pages/man2/fcntl.2.html
     */
    int status = fcntl(*socketDesc, F_SETFL, fcntl(*socketDesc, F_GETFL, 0) | O_NONBLOCK);
    (void)status; // Silence unused variable warning

    // Prepare the sockaddr_in structure
    server->sin_family = AF_INET;
    server->sin_addr.s_addr = INADDR_ANY;
    server->sin_port = htons(DEFAULT_PORT);

    // Bind to port
    if(bind(*socketDesc, (struct sockaddr*)server, sizeof(*server)) < 0)
    {
        perror("ERROR: Failed to bind to port");
        exit(1);
    }
    printf("INFO: Bind done\n");

    // Prepare the server to listen to incoming client connections
    listen(*socketDesc, MAX_CLIENT);
}

void RejectChat(clientData *client, char *returnMessage)
{
    snprintf(returnMessage, DEFAULT_BUFLEN, "TALKTO: %d", (int)IDLE);
    if(client->state != LOGGING_OUT) SendMessage(client, returnMessage);
    if(client->chattingWith)
    {
        SendMessage(client->chattingWith, returnMessage);
        client->chattingWith->state = IDLE;
        client->chattingWith->chattingWith = NULL;
    }
    client->state = IDLE;
    client->chattingWith = NULL;
}

void HandleChatRequests(clientData *client, char *clientMessage, char *returnMessage)
{
    if(client->state == CONNECTING) // If the client sending the request sent a message, it must be to cancel the conversation
    {
        char response[8];
        sscanf(clientMessage, "TalkTo %s", response);
        if(strncmp(response, "Timeout", 7) == 0)
        {
            printf("WARN: Client took too long to respond!\n");
            RejectChat(client, returnMessage);
        }
        return;
    }
    else if(client->state == PENDING_REQUEST)
    {
        char response[7];
        sscanf(clientMessage, "TalkTo %s", response);
        if(strncmp(response, "Accept", 6) == 0)
        {
            client->state = CHATTING;
            client->chattingWith->state = CHATTING;
            snprintf(returnMessage, DEFAULT_BUFLEN, "TALKTO: %d", (int)CHATTING);
            SendMessage(client, returnMessage);
            if(client->chattingWith)
                SendMessage(client->chattingWith, returnMessage);
        }
        else // Rejected, both clients go back to their normal state
        {
            RejectChat(client, returnMessage);
        }
        return;
    }
    else if(client->state != IDLE) // Allow client to request a conversation only when idle
    {
        SendMessage(client, "ERROR: You can't run this command now!");
        return;
    }
    // Load username from client message
    char tempUsername[USERNAME_MAX];
    strncpy(tempUsername, clientMessage+7, USERNAME_MAX);

    if(strlen(tempUsername) == 0)
    {
        SendMessage(client, "TalkTo: Empty username!");
        return;
    }
    if(strcmp(tempUsername, client->username) == 0)
    {
        SendMessage(client, "TalkTo: You can't initiate a conversation with yourself!");
        return;
    }
    clientData* target = ClientDataFind(tempUsername);
    if(!target)
    {
        SendMessage(client, "TalkTo: Couldn't find user!");
        return;
    }
    if(target->state != IDLE)
    {
        SendMessage(client, "TalkTo: User is currently busy, try again later!");
        return;
    }

    // Send state change and the recipient's username back to client
    snprintf(returnMessage, DEFAULT_BUFLEN, "TALKTO: %d %s", (int)CONNECTING, tempUsername);
    SendMessage(client, returnMessage);
    // Send state change and the sender's username to recipient
    memset(returnMessage, 0, DEFAULT_BUFLEN);
    snprintf(returnMessage, DEFAULT_BUFLEN, "TALKTO: %d %s", (int)PENDING_REQUEST, client->username);
    SendMessage(target, returnMessage);
    // Update clients' states server-side
    client->state = CONNECTING;
    client->chattingWith = target;
    target->state = PENDING_REQUEST;
    target->chattingWith = client;
}

void HandleLogin(clientData *client, char *clientMessage, char *returnMessage)
{
    if(client->state != LOGGING_IN)
    {
        SendMessage(client, "ERROR: Already logged in");
        return;
    }
    char tempUsername[USERNAME_MAX];
    strncpy(tempUsername, clientMessage+6, USERNAME_MAX);

    // Empty and already taken usernames are invalid
    int usernameInvalid = strlen(tempUsername) == 0 || strlen(clientMessage+6) >= USERNAME_MAX || (ClientDataFind(tempUsername) != NULL);
    if(usernameInvalid)
    {
        SendMessage(client, "ERROR: Username is taken or invalid!");
    }
    else // If the username is valid, apply changes server-side
    {
        strncpy(client->username, tempUsername, USERNAME_MAX);
        client->state = IDLE;
        snprintf(returnMessage, DEFAULT_BUFLEN, "%d %s", (int)IDLE, tempUsername); // Load the return message with relevant data (client's new state and username)
        SendMessage(client, returnMessage);
    }
    printf("INFO: Login request %s\n", usernameInvalid ? "rejected" : "accepted");
}

void GetUserData(clientData *client, char *returnMessage)
{
    if(client->state == LOGGING_IN)
    {
        SendMessage(client, "ERROR: You're not authorized to run this command!");
        return;
    }
    strcat(returnMessage, "LOG: List of users: \n");
    clientData *clientPtr = clients;
    while(clientPtr != NULL)
    {
        strcat(returnMessage, clientPtr->username);

        if(clientPtr->id == client->id)
            strcat(returnMessage, " (You)"); // Append the (You) indicator to the client querying usernames
        if(clientPtr->state == CHATTING)
            strcat(returnMessage, " (Busy)"); // Append the (Chatting) indicator to busy clients
        if(clientPtr->nextClient)
            strcat(returnMessage, "\n");

        clientPtr = clientPtr->nextClient;
    }
    if(SendMessage(client, returnMessage))
        printf("INFO: Sent user list\n");
}

void SendTo(clientData *client, char *clientMessage, char *returnMessage)
{
    if(client->state != CHATTING)
    {
        SendMessage(client, "ERROR: Client is not in a conversation");
        return;
    }
    snprintf(returnMessage, DEFAULT_BUFLEN, "MESSAGE:[%s]: %s", client->username, clientMessage+5);
    if(client->chattingWith)
    {
        SendMessage(client->chattingWith, returnMessage);
        SendMessage(client, returnMessage);
    }
}
void DisconnectChat(clientData *client, char *returnMessage)
{
    if(client->state != LOGGING_OUT && client->state != CHATTING)
    {
        SendMessage(client, "ERROR: Client is not in a conversation");
        return;
    }
    if(client->chattingWith)
    {
        printf("Disconnecting from conversation...\n");
        snprintf(returnMessage, DEFAULT_BUFLEN, "DISCONNECT: %d", (int)IDLE);
        SendMessage(client->chattingWith, returnMessage);
        SendMessage(client, returnMessage);
        client->chattingWith->state = IDLE;
        client->state = IDLE;
    }
}


#endif //SERVER_H
