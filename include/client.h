#ifndef CLIENT_H
#define CLIENT_H
#include "shared.h"

void SendMessage(int socket, char* message)
{
    if(send(socket, message, strlen(message), 0) < 0)
    {
        perror("Send failed");
        close(socket);
        exit(1);
    }
}
// This is a blocking function as it uses recv
void ReceiveMessage(int socket, int *readSize, char *receivedMessage)
{
    while((*readSize = recv(socket, receivedMessage, DEFAULT_BUFLEN, 0)) > 0) { break; }
    if(*readSize == -1)
    {
        perror("recv: Couldn't receive message from socket");
        close(socket);
        exit(1);
    }
}

void ReadLine(char* message)
{
    fgets(message, DEFAULT_BUFLEN, stdin);
    // Trimming out the newline character, see https://en.cppreference.com/w/c/string/byte/strcspn
    message[strcspn(message, "\n")] = 0;
}

void TryConnect(int *sock, struct sockaddr_in *server, char* ip)
{
    // Create socket
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock == -1)
    {
        printf("Could not create socket\n");
        exit(1);
    }
    printf("Socket created\n");

    // Configuring server information
    server->sin_addr.s_addr = inet_addr(ip);
    server->sin_family = AF_INET;
    server->sin_port = htons(DEFAULT_PORT);

    // Connect to remote server
    if (connect(*sock, (struct sockaddr*)server, sizeof(*server)) < 0)
    {
        perror("Connect failed. Error");
        exit(1);
    }
}

// The following functions are called in the listening thread
void HandleDisconnect(clientStates *clientState, char *chatUsername, char *receivedMessage)
{
    int s;
    sscanf(receivedMessage, "DISCONNECT: %d", &s);
    *clientState = (clientStates)s;
    printf("Disconnected from conversation with %s\n>", chatUsername);
    fflush(stdout);
    memset(chatUsername, 0, USERNAME_MAX);
}
void HandleTalkTo(clientStates *clientState, char *chatUsername, char *receivedMessage)
{
    int s;
    sscanf(receivedMessage, "TALKTO: %d", &s);
    *clientState = (clientStates)s;
    if(*clientState == CHATTING)
    {
        printf("You are now chatting with %s.\n>", chatUsername);
        fflush(stdout);
    }
    else if(*clientState == IDLE)
    {
        printf("Request denied!\n>");
        fflush(stdout);
        memset(chatUsername, 0, USERNAME_MAX);
    }
    else
    {
        strncpy(chatUsername, receivedMessage+10, USERNAME_MAX);
        if(*clientState == CONNECTING) printf("Sent chat request to %s...\n", chatUsername);
        else if(*clientState == PENDING_REQUEST) printf("User %s wants to chat, will you accept? [y/N]\n>", chatUsername);
        fflush(stdout);
    }
}

int AssertResponse(clientStates *clientState, char *receivedMessage)
{
    if(strlen(receivedMessage) == 0)
    {
        printf("Login request failed: empty reply from server.\n");
        *clientState = LOGGING_OUT;
        return 0;
    }
    return 1;
}

#endif //CLIENT_H
