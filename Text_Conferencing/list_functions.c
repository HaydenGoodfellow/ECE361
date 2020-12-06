#include "server.h"

// Doubly linked list which tracks all clients
extern clientList *clients;

// Doubly linked list which tracks all sessions
extern sessionList *sessions; 

//==============================================//
// Functions for session list
//==============================================//
// Initialize session data
session *initSession(char *sessionName) {
    session *newSession = malloc(sizeof(session));
    newSession->name = strdup(sessionName); // Deep copy not shallow copy as the string is on stack
    for (int i = 0; i < MAX_NUM_CLIENTS_IN_SESSION; ++i) 
        newSession->clients[i] = NULL;
    newSession->numClients = 0;
    newSession->nextSession = NULL;
    newSession->prevSession = NULL;
    return newSession;
}

// Add session to the end of the linked list of sessions
void addSessionToList(struct session *newSession) {
    if (newSession == NULL) {
        fprintf(stderr, "Can't add session to list! Session is NULL\n");
        return;
    }
    // If its the only session 
    if (sessions->numSessions == 0) {
        sessions->frontSession = newSession;
        sessions->backSession = newSession;
    }
    else { // Append to end
        sessions->backSession->nextSession = newSession;
        newSession->prevSession = sessions->backSession;
        sessions->backSession = newSession;
    }
    ++sessions->numSessions;
}

// Add client to list and begin polling for data from that client
void addClientToSession(client *newClient, session *sess) {

}

//==============================================//
// Functions for client list
//==============================================//
// Initialize client data for newly connected client
client *initClient(char *name, int clientfd) {
    client *newClient = malloc(sizeof(client));
    newClient->name = strdup(name); // Deep copy not shallow copy as the string is on stack
    newClient->password = NULL;
    newClient->loggedIn = false;
    newClient->clientfd = clientfd;
    newClient->nextClient = NULL;
    newClient->prevClient = NULL;
    return newClient;
}