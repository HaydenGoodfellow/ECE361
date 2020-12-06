#include "server.h"

// Doubly linked list which tracks all clients
extern ClientList *clients;

// Doubly linked list which tracks all sessions
extern SessionList *sessions; 

//==============================================//
// Functions for session list
//==============================================//
// Initialize session data
Session *initSession(char *sessionName) {
    Session *newSession = malloc(sizeof(Session));
    newSession->name = strdup(sessionName); // Deep copy not shallow copy as the string is on stack
    for (int i = 0; i < MAX_NUM_CLIENTS_IN_SESSION; ++i) 
        newSession->clients[i] = NULL;
    newSession->numClients = 0;
    newSession->nextSession = NULL;
    newSession->prevSession = NULL;
    return newSession;
}

// Add session to the end of the linked list of sessions
void addSessionToList(Session *newSession) {
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
void addClientToSession(Client *newClient, Session *session) {
    if (session->numClients < MAX_NUM_CLIENTS_IN_SESSION) {
        session->clients[session->numClients] = newClient;
        ++session->numClients;
        updatePollfds(session);
    }
    else {
        fprintf(stderr, "Error adding client: Session is full!\n");
    }
}

// Remove client from session list. Delete session if no more clients
void removeClientFromSession(Client *client, Session *session) {

}

// Update the pollfds array to remove clients that left or include clients that joined
void updatePollfds(Session *session) {
    // Clear current fds array
    memset(session->clientFds, 0, sizeof(session->clientFds));
    // Add current clients to fds array
    for (int i = 0; i < session->numClients; ++i) {
        session->clientFds[i].fd = session->clients[i]->clientfd;
        session->clientFds[i].events = POLLIN;
    } 
}

// Search for a session with a given name. Returns NULL if not found
Session *searchForSession(char *sessionName) {
    if (sessionName == NULL)
        return NULL;
    for (Session *search = sessions->frontSession; search != NULL; search = search->nextSession) {
        if (strcmp(search->name, sessionName) == 0) 
            return search;
    }
    return NULL;
}

//==============================================//
// Functions for client list
//==============================================//
// Initialize client data for newly connected client
Client *initClient(char *name, int clientfd) {
    Client *newClient = malloc(sizeof(Client));
    newClient->name = strdup(name); // Deep copy not shallow copy as the string is on stack
    newClient->password = NULL;
    newClient->loggedIn = false;
    newClient->clientfd = clientfd;
    newClient->nextClient = NULL;
    newClient->prevClient = NULL;
    return newClient;
}

// Add session to the end of the linked list of clients
void addClientToList(Client *newClient) {
    if (newClient == NULL) {
        fprintf(stderr, "Can't add client to list! Client is NULL\n");
        return;
    }
    // If its the only session 
    if (clients->numClients == 0) {
        clients->frontClient = newClient;
        clients->backClient = newClient;
    }
    else { // Append to end
        clients->backClient->nextClient = newClient;
        newClient->prevClient = clients->backClient;
        clients->backClient = newClient;
    }
    ++clients->numClients;
}