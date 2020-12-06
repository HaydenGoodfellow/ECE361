#include "server.h"

// Doubly linked list which tracks all clients
extern struct clientList *clients;

// Doubly linked list which tracks all sessions
extern struct sessionList *sessions; 

//==============================================//
// Functions for session list
//==============================================//
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

//==============================================//
// Functions for client list
//==============================================//