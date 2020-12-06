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

#define MAX_NUM_CLIENTS_IN_SESSION 64

// Stuct for storing client data. Mostly needed for the name 
typedef struct client client;
struct client {
    char *name;
    char *password; // Super not secure, only doing this for project
    bool loggedIn;
    // File descriptor for communication with client
    int clientfd;
    // Pointers for the doubly linked list
    client *nextClient;
    client *prevClient;
};

// Doubly linked list of all clients
typedef struct clientList {
    client *frontClient;
    client *backClient;
    unsigned numClients;
    pthread_mutex_t clientListLock;
} clientList;

// Struct for session data
typedef struct session session;
struct session {
    pthread_t* thread;
    char *name;
    struct pollfd clientFds[MAX_NUM_CLIENTS_IN_SESSION]; 
    // Array which stores pointers to the client data stored in client list
    client *clients[MAX_NUM_CLIENTS_IN_SESSION];
    unsigned numClients;
    // Pointers for the doubly linked list
    session *nextSession;
    session *prevSession;
};

// Doubly linked list of active sessions
typedef struct sessionList {
    session *frontSession;
    session *backSession;
    unsigned numSessions;
    pthread_mutex_t sessionListLock;
} sessionList;

//==============================================//
// Server core functions (server.c)
//==============================================//
// Poll for data from clients in a given session. One thread for each session
void *pollSession(void *sessionNum);

// Session 0 (meta session) is for users who are not in a session and it only accepts commands
void *pollMetaSession();

// Perform command given by a client
void performCommand(message *msg, unsigned sNum, unsigned clientNum);

//==============================================//
// Communication functions (server.c)
//==============================================//
// Create and open socket. Set it as listener. Returns socket file descriptor
int establishConnection(char *portNum);

// Send message to all connected clients
void broadcastMessage(char *messageAsString, unsigned strLength, unsigned sNum, unsigned clientNum);

// Send message to specific client
void sendResponse(messageTypes type, char *response, unsigned sNum, unsigned clientNum);

//==============================================//
// Serialization functions (server.c)
//==============================================//
// Converts string from socket to message type for analysis
message *parseMessageAsString(char *input);

// Convert message to string. Return size in size parameter
char *messageToString(message *msg, unsigned *size);

//==============================================//
// Functions for session list (list_functions.c)
//==============================================//
// Initialize session data
session *initSession(char *sessionName);

// Add session to the end of the linked list of sessions
void addSessionToList(session *newSession);

// Add client to list and begin polling for data from that client
void addClientToSession(client *newClient, session *sess);

// Remove client from session list. Delete session if no more clients
void removeClientFromSession(unsigned sNum, unsigned clientNum);

//==============================================//
// Functions for client list (list_functions.c)
//==============================================//
// Initialize session data
client *initClient(char *name, int clientfd);

// Add session to the end of the linked list of sessions
void addClientToList(client *newClient);


#endif // SERVER_H