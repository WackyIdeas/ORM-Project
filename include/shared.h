#ifndef SHARED_H
#define SHARED_H
// Headers
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

/*
 * Header file that is used both in server and client code.
 */

#define DEFAULT_BUFLEN 65536
#define DEFAULT_PORT   27015

#define MAX_CLIENT 5
// A username can be at most 15 characters long
#define USERNAME_MAX 16
// A command sent to the server can be at most 10 characters long
#define CLIENT_CMD_MAX 11

typedef enum { LOGGING_IN, LOGGING_OUT, IDLE, CONNECTING, PENDING_REQUEST, CHATTING } clientStates;
typedef enum { LOGIN = 0, LOGOUT, USERS, TALKTO, DISCONNECT, DATA, UNKNOWN } clientCommands;

// nanosleep boilerplate used to delay threads in order to prevent busy-waiting
struct timespec timeout = { .tv_sec = 0, .tv_nsec = 500000 };

// Helper functions to reduce copy-pasted code
int startsWith(char *message, char *str)
{
    return strncmp(message, str, strlen(str)-2) == 0;
}

int AssertValidCommand(clientCommands c)
{
    if(c == UNKNOWN)
    {
        printf("Invalid command!\n>");
        return 0;
    }
    return 1;
}

// Used to convert a string command into its enum counterpart
char *clientCommandsString[6] = {"Login", "Logout", "Users", "TalkTo", "Disconnect", "Data" };
clientCommands StringToCommandClient(char *message)
{
    char command[CLIENT_CMD_MAX];
    sscanf(message, "%10s\n", command);

    int i;
    for(i = 0; i < 6; i++)
    {
        if(strcmp(command, clientCommandsString[i]) == 0)
        {
            return (clientCommands)i;
        }
    }
    return UNKNOWN;
}

#endif // SHARED_H
