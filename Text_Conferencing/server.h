#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <signal.h>
#include <poll.h>
#include <pthread.h>

#include "message.h"

#define MAX_SOCKET_INPUT_SIZE 2048
#define MAX_SESSIONS_PER_CLIENT 64
#define MAX_CLIENTS_IN_SESSION 64

// Forward declarations to avoid unknown type name errors
typedef struct Client Client;
typedef struct Session Session;

// Stuct for storing client data. Mostly needed for the name 
struct Client {
    char *name;
    char *password; // Super not secure, only doing this for project
    bool loggedIn;
    // File descriptor for communication with client
    int clientfd;
    // Array which stores pointers to the sessions which the client is in
    Session *sessions[MAX_SESSIONS_PER_CLIENT];
    unsigned numSessions;
    // Session the client is currently sending messages to
    Session *talkingToSession;
    Session *prevTalkingTo; // Session it was talking to last
    // Pointers for the doubly linked list
    Client *nextClient;
    Client *prevClient;
};

// Doubly linked list of all clients
typedef struct ClientList {
    Client *frontClient; 
    Client *backClient;
    unsigned numClients;
    pthread_mutex_t clientListLock;
} ClientList;

// Struct for session data
struct Session {
    pthread_t* thread;
    char *name;
    struct pollfd clientFds[MAX_CLIENTS_IN_SESSION]; 
    // Array which stores pointers to the client data stored in client list
    Client *clients[MAX_CLIENTS_IN_SESSION];
    unsigned numClients;
    unsigned numTalking; // Number of clients currently sending to this session
    // Pointers for the doubly linked list
    Session *nextSession;
    Session *prevSession;
    // Flag for if session is deleted and thread should exit
    volatile int sessionDeleted;
};

// Doubly linked list of active sessions
typedef struct SessionList {
    Session *frontSession; // Will always be meta session
    Session *backSession;
    unsigned numSessions;
    pthread_mutex_t sessionListLock;
    // Since meta session can have no member we don't want it to spin while waiting for one
    // So use a monitor to wake it up if it slept when it had no members
    pthread_mutex_t metaSessionLock;
    pthread_cond_t metaSessionHasMembers;
} SessionList;

//==============================================//
// Server core functions (server.c)
//==============================================//
// Create and open socket. Set it as listener. Returns socket file descriptor
int establishConnection(char *portNum);

// Poll for data from clients in a given session. One thread for each session
void *pollSession(void *sessionPtr);

// Session 0 (meta session) is for users who are not in a session and it only accepts commands
void *pollMetaSession(void *metaSessionPtr);

//==============================================//
// Serialization functions (server.c)
//==============================================//
// Converts string from socket to message type for analysis
message *parseMessageAsString(char *input);

// Convert message to string. Returns size in size parameter
char *messageToString(message *msg, unsigned *size);

//==============================================//
// Communication functions (server.c)
//==============================================//
// Send message to all connected clients
void broadcastMessage(char *messageAsString, unsigned strLength, Session *session, int senderfd);

// Send message to specific client
void sendResponse(messageTypes type, char *response, Client *client);

// Perform command given by a client
void performCommand(message *msg, Session *session, Client *client);

//==============================================//
// Functions for sessions (list_functions.c)
//==============================================//
// Initialize session data
Session *initSession(char *sessionName);

// Add session to the end of the linked list of sessions
void addSessionToList(Session *newSession);

// Removes session from the linked list
void removeSessionFromList(Session *session);

// Add client to session and begin polling for data. Removes client from meta session if in it
void addClientToSession(Client *newClient, Session *session);

// Remove client from session list. Deletes session if no more clients
void removeClientFromSession(Client *client, Session *session);

// Update the pollfds array to remove clients that left or include clients that joined
void updatePollfds(Session *session);

// Search for a session with a given name. Returns NULL if not found
Session *searchForSession(char *sessionName);

//==============================================//
// Functions for client objects (list_functions.c)
//==============================================//
// Initialize session data
Client *initClient(char *name, int clientfd);

// Add session to the end of the linked list of clients
void addClientToList(Client *newClient);

// Get a client's object from a session given their socket file descriptor
Client *getClientByFd(int clientfd, Session *session);

// Search for a client in a specifc session. Returns false if not found
bool clientIsInSession(Client *client, Session *session);

// Find client with a given name if they're logged in currently logged in
Client *clientExists(char *name);

//==============================================//
// Functions for checking logins (login_functions.c)
//==============================================//
// Ensure that the user information given matches what is store in database
bool checkValidLogin(char* clientID, char* password, char *error);

#endif // SERVER_H