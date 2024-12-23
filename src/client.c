#include "client.h"

                                       // Global variables shared by the main thread and listener thread
clientStates clientState = LOGGING_IN; // Client state provided by the server
char clientUsername[USERNAME_MAX];
char chatUsername[USERNAME_MAX];
pthread_t listener;                   // Used for monitoring incoming messages from the server or other clients
char receivedMessage[DEFAULT_BUFLEN]; // Message buffer used for receiving from the server
int readSize;
int sock;                             // Socket file descriptor

/*
 * Listening is required to be in another thread in order to handle conversation requests,
 * as well as conversation messages themselves.
 */
void MessageListener()
{
    while(clientState != LOGGING_OUT)
    {
        memset(receivedMessage, 0, DEFAULT_BUFLEN);
        ReceiveMessage(sock, &readSize, receivedMessage);

        AssertResponse(&clientState, receivedMessage);
        /*
         * Server-side responses start with the following strings, they can be simply printed out to the client.
         * Errors from the TalkTo command require resetting the local clientState back to IDLE.
         */
        int isError = startsWith(receivedMessage, "ERROR:");
        int isLog = startsWith(receivedMessage, "LOG:");
        int isTalkTo = startsWith(receivedMessage, "TalkTo:"); // An error message from the command TalkTo
        if(isError || isLog || isTalkTo)
        {
            if(isTalkTo) clientState = IDLE;                   // Set client to known state
            printf("%s\n>", receivedMessage);
            fflush(stdout);
        }
        else if(startsWith(receivedMessage, "MESSAGE:"))       // Received message from another user
        {
            printf("%s\n>", receivedMessage+8);
            fflush(stdout);
        }
        else if(startsWith(receivedMessage, "DISCONNECT:"))
        {
            HandleDisconnect(&clientState, chatUsername, receivedMessage);
        }
        else if(startsWith(receivedMessage, "TALKTO:"))
        {
            HandleTalkTo(&clientState, chatUsername, receivedMessage);
        }
    }
}

void RequestLogin(int socket, char* message, int* readSize, char* receivedMessage)
{
    SendMessage(socket, message);
    ReceiveMessage(socket, readSize, receivedMessage);

    AssertResponse(&clientState, receivedMessage);
    if(startsWith(receivedMessage, "ERROR:")) // Invalid username
    {
        printf("%s\n", receivedMessage);
        return;
    }

    int s;
    sscanf(receivedMessage, "%d", &s);                        // Read expected number from the returned message
    clientState = (clientStates)s;

    /*
     * Upon successful login, the user needs to be able to get a listening thread
     * in order to properly communicate with the server and other clients.
     */
    pthread_create(&listener, NULL, (void*)MessageListener, NULL);
    pthread_detach(listener);
    strncpy(clientUsername, receivedMessage+2, USERNAME_MAX); // Copy the rest of the returned message into the client's username
    printf("Login successful! Your username is %s\n>", clientUsername);
}

int main(int argc , char *argv[])
{
    struct sockaddr_in server;          // Server information for connecting
    char message[DEFAULT_BUFLEN];       // Message buffer used for sending to the server

    char *ip;                           // Allow setting arbitrary IP address. If the user doesn't provide any address, localhost is used as a fallback
    if(argc == 2) ip = argv[1];
    else ip = "127.0.0.1";
    int shouldClose = 0;

    int maxTimeout = 0;                 // Used to manage timeouts in the client to prevent waiting indefinitely while attempting to establish a conversation
    timeout.tv_sec = 2;                 // Set sleep tick to 2 seconds
    timeout.tv_nsec = 0;

    TryConnect(&sock, &server, ip);
    printf("Connected\n");

    printf("Available commands:\n\tLogin [username] - Log in using a unique username\n\t"
            "Logout - Logs out and exits the application\n\t"
            "Users - List all users\n\t"
            "TalkTo [username] - Open conversation with user\n\t"
            "Disconnect - Disconnects currently opened conversation\n\t"
            "Data [message] - Send message to user in current conversation\n>");

    while(!shouldClose)                                                // Main event loop where the server and client exchange messages
    {
        clientCommands cmd;
        if(clientState != LOGGING_OUT && clientState != CONNECTING)
            ReadLine(message);

        switch(clientState)
        {
            case LOGGING_IN:                                           // Login state, before we're registered as a user, here we are still in a single-thread environment
                cmd = StringToCommandClient(message);
                if(!AssertValidCommand(cmd)) continue;
                switch(cmd)
                {
                    case LOGIN:
                        RequestLogin(sock, message, &readSize, receivedMessage);
                        break;
                    case LOGOUT:
                        clientState = LOGGING_OUT;
                        break;
                    default:
                        SendMessage(sock, message);
                        ReceiveMessage(sock, &readSize, receivedMessage);
                        if(AssertResponse(&clientState, receivedMessage))
                            printf("%s\n>", receivedMessage);
                        break;
                }
                break;
            case IDLE:                                                  // Logged in, but not in a conversation or attempting to initiate one
                cmd = StringToCommandClient(message);
                if(!AssertValidCommand(cmd)) continue;
                switch(cmd)
                {
                    case USERS:                                         // Command authorization is done server-side
                    case LOGIN:
                    case DISCONNECT:
                    case DATA:
                        SendMessage(sock, message);
                        break;
                    case TALKTO:
                        clientState = CONNECTING;                       // To prevent client from attempting to read from stdin, this is required for the timeout to work properly
                        SendMessage(sock, message);
                        break;
                    case LOGOUT:
                        clientState = LOGGING_OUT;
                        break;
                    default:
                        break;
                }
                break;
            case CHATTING:
                cmd = StringToCommandClient(message);
                if(!AssertValidCommand(cmd)) continue;
                switch(cmd)
                {
                    case TALKTO:
                    case LOGIN:
                    case USERS:
                    case DISCONNECT:
                    case DATA:
                        SendMessage(sock, message);
                        break;
                    case LOGOUT:
                        clientState = LOGGING_OUT;
                        break;
                    default:
                        break;
                }
                break;
            case CONNECTING:
                nanosleep(&timeout, NULL);
                if(clientState == CONNECTING)                // Check if the client is still in the CONNECTING state after sleep
                {
                    printf(".");                             // Indicator that tells the user the conversation is in the process of being established
                    fflush(stdout);
                    maxTimeout += 2;
                    if(maxTimeout == 30)
                    {
                        maxTimeout = 0;
                        SendMessage(sock, "TalkTo Timeout"); // Cancel conversation upon timeout
                    }
                }
                break;
            case PENDING_REQUEST:
                cmd = StringToCommandClient(message);
                if(cmd == LOGOUT)
                {
                    clientState = LOGGING_OUT;
                    break;
                }
                // Implicitly treat any input other than Y/y as rejection
                SendMessage(sock, (message[0] == 'Y' || message[0] == 'y') ? "TalkTo Accept" : "TalkTo Reject");
                break;
            case LOGGING_OUT: // Client is logging out, or is being logged out by the server
                printf("Closing socket\n>");
                close(sock);
                shouldClose = 1;
                break;
            default:
                break;
        }
        memset(message, 0, DEFAULT_BUFLEN);
        memset(receivedMessage, 0, DEFAULT_BUFLEN);
    }
    return 0;
}

