#include "server.h"

// Rename mutex functions to simple names for readability
#define lock(x) pthread_mutex_lock(x) 
#define unlock(x) pthread_mutex_unlock(x) 
#define wait(x, y) pthread_cond_wait(x, y) 
#define signal_cond(x) pthread_cond_signal(x) 
#define broadcast(x) pthread_cond_broadcast(x) 

// Doubly linked list which tracks all clients
extern ClientList *clients;

// Doubly linked list which tracks all sessions
extern SessionList *sessions; 

// Static functions limited to this file's scope
// Remove data on specific client stored in session object
static int removeClientDataFromSession(Client *client, Session *session);
// Remove data on a session from a client's object
static int removeSessionDataFromClient(Session *session, Client *client);
// Delete a session and remove that session from the list
static void deleteSession(Session *session);

//==============================================//
// Functions for session list
//==============================================//
// Initialize session data
Session *initSession(char *sessionName) {
    Session *newSession = malloc(sizeof(Session));
    newSession->name = strdup(sessionName); // Deep copy not shallow copy as the string is on stack
    for (int i = 0; i < MAX_CLIENTS_IN_SESSION; ++i) 
        newSession->clients[i] = NULL;
    newSession->numClients = 0;
    newSession->numTalking = 0;
    newSession->nextSession = NULL;
    newSession->prevSession = NULL;
    newSession->sessionDeleted = 0;
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
    // Check if the client is already in the session, if so change their talking to pointer
    if (clientIsInSession(newClient, session)) {
        fprintf(stderr, "Client \"%s\" is already in session \"%s\". Updating talking\n", newClient->name, session->name);
        Session *oldSession = newClient->talkingToSession;
        newClient->talkingToSession = session;
        newClient->prevTalkingTo = oldSession;
        updatePollfds(session);
        updatePollfds(oldSession);
        return;
    }
    bool addingToMetaSession = (sessions->frontSession == session);
    fprintf(stderr, "Adding client %s to session %s\n", newClient->name, session->name);
    if ((newClient->numSessions < MAX_SESSIONS_PER_CLIENT) && (session->numClients < MAX_CLIENTS_IN_SESSION)) {
        if (!addingToMetaSession) { // We don't add the meta session's data to clients
            // Add session pointer to the client data
            fprintf(stderr, "Adding session \"%s\" data to client \"%s\"\n", session->name, newClient->name);
            newClient->sessions[newClient->numSessions] = session;
            ++newClient->numSessions;
        }
        // Add client pointer to the session data
        session->clients[session->numClients] = newClient;
        if (newClient->talkingToSession)
            fprintf(stderr, "Currently talking to: %s\n", newClient->talkingToSession->name);
        Session *oldSession = newClient->talkingToSession;
        newClient->talkingToSession = session;
        newClient->prevTalkingTo = oldSession;
        if (newClient->prevTalkingTo)
            fprintf(stderr, "New session: %s. Old session: %s\n", newClient->talkingToSession->name, newClient->prevTalkingTo->name);
        ++session->numClients;
        updatePollfds(session);
        if (oldSession != NULL && !addingToMetaSession)
            updatePollfds(oldSession);
    }
    else if (newClient->numSessions > MAX_SESSIONS_PER_CLIENT) {
        fprintf(stderr, "Error adding session to client: Client is already in max number of sessions!\n");
    }
    else {
        fprintf(stderr, "Error adding client: Session is full!\n");
    }
    // If client is in a session they can't also be in the meta session
    if (!addingToMetaSession) {
        removeClientFromSession(newClient, sessions->frontSession);
    }
    else {
        lock(&sessions->metaSessionLock);
        // Signal that the meta session has clients in case the thread is sleeping
        signal_cond(&sessions->metaSessionHasMembers);
        unlock(&sessions->metaSessionLock);
        // Ensure client has no sessions if they're in the meta session
        assert(newClient->numSessions == 0);
    }
}

// Remove client from session list and the session from the clients list
void removeClientFromSession(Client *client, Session *session) {
    bool removeFromMetaSession = (sessions->frontSession == session);
    // Check if session is empty or NULL
    if (!session || (session->numClients == 0)) {
        if (!removeFromMetaSession)
            fprintf(stderr, "Error removing client: Session is empty or NULL\n");
        return;
    }
    // First determine if they're in session
    if (!clientIsInSession(client, session)) {
        if (!removeFromMetaSession)
            fprintf(stderr, "Error removing client: Client \"%s\" is not in session \"%s\"\n", client->name, session->name);
        return;
    }
    fprintf(stderr, "Removing client \"%s\" from session \"%s\" with num clients: %d\n", client->name, session->name, session->numClients);
    // Remove client data from session
    int ret = removeClientDataFromSession(client, session);
    assert(ret == 0);
    // Remove session data from client if its not the meta session
    if (!removeFromMetaSession) {
        ret = removeSessionDataFromClient(session, client);
        assert(ret == 0);
        if (client->numSessions == 0) { // If its in no sessions add it to the meta one
            fprintf(stderr, "Adding client \"%s\" back to meta sessions as they are no longer in any sessions\n", client->name);
            addClientToSession(client, sessions->frontSession);
            client->prevTalkingTo = NULL;
        }
        else { // Switch who the client is talking to
            if (client->prevTalkingTo) {
                fprintf(stderr, "Switching client \"%s\" to talk in session %s\n", client->name, client->prevTalkingTo->name);
                client->talkingToSession = client->prevTalkingTo;
                client->prevTalkingTo = NULL;
            }
            else {
                fprintf(stderr, "Switching client \"%s\" with no previous session to talk in session %s\n", client->name, client->sessions[client->numSessions - 1]->name);
                client->talkingToSession = client->sessions[client->numSessions - 1];
                client->prevTalkingTo = NULL;
            }
            updatePollfds(client->talkingToSession);
        }
    }
    // If session is empty and not the meta session then we delete it
    if ((session->numClients == 0) && (!removeFromMetaSession)) {
        deleteSession(session);
        return;
    }
    // Update the fds to no longer poll from removed client
    updatePollfds(session);    
}

// Remove data on specific client stored in session object
static int removeClientDataFromSession(Client *client, Session *session) {
    // fprintf(stderr, "Removing data of client \"%s\" from session \"%s\"\n", client->name, session->name);
    if (session->clients[session->numClients - 1] == client) { // If the client is at the end just remove them
        session->clients[session->numClients - 1] = NULL;
        --session->numClients;
    }
    else {  // Client is somewhere the session array so remove and update pointers
        bool clientFound = false;
        for (int i = 0; i < session->numClients - 1; ++i) {
            if (session->clients[i] == client)
                clientFound = true;
            if (clientFound)  // Only move clients past the client we're removing
                session->clients[i] = session->clients[i + 1]; // Shift clients left
        }
        if (clientFound) {
            session->clients[session->numClients] = NULL; // Last element may be duplicated so set to NULL
            --session->numClients;
        }
        else { // Client somehow wasn't found in array despite the earlier function saying they are
            fprintf(stderr, "Error: Client \"%s\" not found in session's list despite passing test at start!\n", client->name);
            return -1; 
        }
    }
    return 0;
}

// Remove data on a session from a client's object
static int removeSessionDataFromClient(Session *session, Client *client) {
    // fprintf(stderr, "Removing data of session \"%s\" from client \"%s\" with numSessions %d\n", session->name, client->name, client->numSessions);
    if (client->sessions[client->numSessions - 1] == session) { // If the session is at the end just remove them
        // fprintf(stderr, "Session is at back of client's list\n");
        client->sessions[client->numSessions - 1] = NULL;
        --client->numSessions;
    }
    else { // Session is somewhere the client array so remove and update pointers
        bool sessionFound = false;
        for (int i = 0; i < client->numSessions - 1; ++i) {
            if (client->sessions[i] == session) 
                sessionFound = true;
            if (sessionFound)
                client->sessions[i] = client->sessions[i + 1];
        }
        if (sessionFound) {
            client->sessions[client->numSessions] = NULL; // Last element may be duplicated so set to NULL
            --client->numSessions;
        }
        else { // Client somehow wasn't found in array despite the earlier function saying they are
            fprintf(stderr, "Error: Session \"%s\" not found in client's list despite passing test at start!\n", session->name);
            return -1; 
        }
    }
    return 0;
}

// Delete session if it has no more clients in it
static void deleteSession(Session *session) {
    assert(session->numClients == 0); // Ensure the session is empty
    assert(session != sessions->frontSession); // Ensure we're not deleting meta session
    fprintf(stderr, "Deleting session: %s\n", session->name);
    // Remove pointers to this session from the doubly linked list
    if (session == sessions->backSession) {  // If session is at end
        assert(session->nextSession == NULL);
        session->prevSession->nextSession = NULL;
        sessions->backSession = session->prevSession;
    }   
    else { // Must be in the middle of linked list
        session->prevSession->nextSession = session->nextSession;
        session->nextSession->prevSession = session->prevSession;
    }
    // Mark session as deleted so the thread exits when it returns
    session->sessionDeleted = 1;
    // Data is cleared by thread when it returns
}

// Update the pollfds array to remove clients that left or include clients that joined
void updatePollfds(Session *session) {
    if (session == NULL)
        return;
    fprintf(stderr, "Updating pollfds for session %s. Clients talking: ", session->name);
    // Clear current fds array
    memset(session->clientFds, 0, sizeof(session->clientFds));
    // Add current clients to fds array
    int arrayIndex = 0;
    for (int i = 0; i < session->numClients; ++i) {
        if (session->clients[i]->talkingToSession == session) {
            fprintf(stderr, "%s ", session->clients[i]->name);
            session->clientFds[arrayIndex].fd = session->clients[i]->clientfd;
            session->clientFds[arrayIndex].events = POLLIN;
            ++arrayIndex;
        }
    } 
    session->numTalking = arrayIndex;
    fprintf(stderr, "\nNum talking in session %s: %d\n", session->name, session->numTalking);
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
    newClient->numSessions = 0;
    newClient->talkingToSession = NULL;
    newClient->prevTalkingTo = NULL;
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

// Search for a client in a specifc session. Returns false if not found
bool clientIsInSession(Client *client, Session *session) {
    if (session->numClients == 0)
        return false;
    for (int i = 0; i < session->numClients; ++i) {
        if (session->clients[i] == client)
            return true;
    }
    return false;
}