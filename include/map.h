#ifndef MAP_H
#define MAP_H
#include "shared.h"

/*
 * Structure that contains information about clients
 * connected to the server:
 *      - ID: Unique identifier
 *      - Socket: Socket file descriptor
 *      - Thread
 *      - Username: Unique display name visible to
 *                  other clients
 *      - State
 *      - Reference to another client in a conversation
 *        with each other.
 */
typedef struct clientData {
    unsigned long id;
    int clientSocket;
    pthread_t clientThread;
    char username[USERNAME_MAX];
    clientStates state;
    struct clientData *chattingWith;
    struct clientData *nextClient;
} clientData;
/*
 * Using a singly-linked list for storing an arbitrary amount of clients,
 * which also makes accessing client information easy through pointers.
 */
unsigned long lastId = 0;
clientData* clients;
unsigned int clientsLen = 0;
pthread_mutex_t clientsLenAccess;

int ClientDataInit()
{
    // Initialize mutex for thread-safe access
    pthread_mutex_init(&clientsLenAccess, NULL);
    return 0;
}
int ClientDataDestroy()
{
    pthread_mutex_destroy(&clientsLenAccess);

    // We must free every element from memory iteratively
    clientData *c = clients;
    while(c != NULL)
    {
        clientData *prev = c;
        c = prev->nextClient;
        free(prev);
    }
    return 0;
}
clientData* ClientDataFind(char username[USERNAME_MAX])
{
    clientData* c = clients;
    while(c != NULL)
    {
        if(strcmp(username, c->username) == 0) return c;
        c = c->nextClient;
    }
    return NULL;
}
int ClientDataRemove(clientData *client)
{
    clientData* prev = NULL;
    clientData* c = clients;

    if(!clients) return 0;
    if(client == clients)
    {
        clients = clients->nextClient;
    }
    else
    {
        // Search for the node that precedes the queried client
        while(c != NULL)
        {
            if(c->nextClient == client)
            {
                prev = c;
                break;
            }
            c = c->nextClient;
        }
        prev->nextClient = client->nextClient;
    }

    free(client);
    pthread_mutex_lock(&clientsLenAccess);
    clientsLen--;
    pthread_mutex_unlock(&clientsLenAccess);
    return 1;
}
clientData* ClientDataAdd(int socket)
{
    clientData *newClient = (clientData*)malloc(sizeof(clientData));
    if(!newClient)
    {
        perror("ERROR: Failed to create new client");
        return NULL;
    }

    if(!clients) // List is empty
    {
        clients = newClient;
    }
    else
    {
        newClient->nextClient = clients;
        clients = newClient;
    }

    pthread_mutex_lock(&clientsLenAccess);
    clientsLen++;
    newClient->id = lastId;
    lastId++;
    pthread_mutex_unlock(&clientsLenAccess);

    newClient->clientSocket = socket;
    newClient->state = LOGGING_IN;
    newClient->username[0] = 0;
    newClient->chattingWith = NULL;
    return newClient;
}

#endif // MAP_H
