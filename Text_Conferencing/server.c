#include "server.h"

// Parts of this code was adapted from Beej's Guide to Network Programming
// Found online at: https://beej.us/guide/bgnet/html/

// Rename mutex functions to simple names for readability
#define lock(x) pthread_mutex_lock(x) 
#define unlock(x) pthread_mutex_unlock(x) 
#define wait(x, y) pthread_cond_wait(x, y) 
#define signal_cond(x) pthread_cond_signal(x) 
#define broadcast(x) pthread_cond_broadcast(x) 

// Doubly linked list which tracks all clients
ClientList *clients;

// Doubly linked list which tracks all sessions
SessionList *sessions; 

//==============================================//
// Main function
//==============================================//
int main(int argc, char **argv) {
	if (argc < 2 || (atoi(argv[1]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using \"server <TCP Listen Port>\"\n");
        return 0;
    }
    char *portNum = argv[1];
    // Create socket and make it a listening socket
    int listenSock = establishConnection(portNum);

    // Allocate heap memory for client list and session list
    clients = malloc(sizeof(ClientList));
    assert(clients != NULL);
    // Initialize mutex locks and monitors
    int ret = pthread_mutex_init(&clients->clientListLock, NULL);
    assert(ret == 0);
    sessions = malloc(sizeof(SessionList));
    assert(sessions != NULL);
    ret = pthread_mutex_init(&sessions->sessionListLock, NULL);
    assert(ret == 0);
    ret = pthread_mutex_init(&sessions->metaSessionLock, NULL);
    assert(ret == 0);
    ret = pthread_cond_init(&sessions->metaSessionHasMembers, NULL);
    assert(ret == 0);

    // Create meta session for people not in a session (Doesn't allow talking, only commands)
    Session *metaSession = initSession("Meta Session");
    addSessionToList(metaSession);
    volatile int threadForMetaSession = 0;

    // Set up fd we will poll and accept other fds from
    struct pollfd fds[1];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = listenSock;
    fds[0].events = POLLIN;
    // Wait for new clients to connect to the socket
    struct sockaddr clientAddr;
    socklen_t clientAddrLen;
    while (true) {
        int pollRet = poll(fds, 1, -1);
        if (pollRet == 0)
            continue;
        assert(fds[0].revents & POLLIN); // Ensure read ready
        fprintf(stderr, "Received connection on listener socket\n");
        clientAddrLen = sizeof(clientAddr);
        int clientfd = accept(listenSock, (struct sockaddr *)&clientAddr, (socklen_t *)&clientAddrLen);
        // Create client object for newly connected client with unknown name
        Client *newClient = initClient("Unknown", clientfd);
        addClientToSession(newClient, metaSession);
        // Create thread for meta session if one doesn't exist yet
        if (!threadForMetaSession) {
            fprintf(stderr, "Creating thread for meta session\n");
            metaSession->thread = malloc(sizeof(pthread_t));
            pthread_create((pthread_t *) metaSession->thread, NULL, (void *) pollMetaSession, (void *) metaSession);
            threadForMetaSession = 1;
        }
    }
    close(listenSock);
    return 0;
}

//==============================================//
// Server core functions 
//==============================================//
// Create and open socket. Set it as listener. Returns socket file descriptor
int establishConnection(char *portNum) {
    struct addrinfo hints, *serverInfo, *ptr;  
    // Set up hints struct
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET; // Ensures IPv4
    hints.ai_socktype = SOCK_STREAM; // Ensures TCP
    // Get address info
    int getAddrReturn = getaddrinfo(NULL, portNum, &hints, &serverInfo);
    if (getAddrReturn != 0) {
        fprintf(stderr, "Failed to get address info: %s\n", gai_strerror(getAddrReturn));
        exit(0);
    }
    // Set up socket
    fprintf(stderr, "Setting up socket\n");
    int listenSock = socket(AF_INET /*IPv4*/, SOCK_STREAM /*TCP*/, 0);
    if (listenSock < 0) {
        fprintf(stderr, "Failed to create socket\n");
        exit(0);
    }
    int optionVal = 1;
    if (setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const void *)&optionVal, sizeof(int)) < 0)
        perror("setsockopt error");
    // Bind socket
    fprintf(stderr, "Binding socket\n");
    ptr = serverInfo;
    while (ptr != NULL) {
        int bindReturn = bind(listenSock, ptr->ai_addr, ptr->ai_addrlen);
        if (bindReturn < 0) {
            fprintf(stderr, "Failed to bind socket\n");
            ptr = ptr->ai_next;
            continue;
        }
        else
            break;
    }
    assert(ptr != NULL); // Wasn't able to bind socket so exit program
    // Set socket as listening socket
    fprintf(stderr, "Listening socket\n");
    if (listen(listenSock, 16) < 0) {
        perror("Listen error");
        exit(0);
    }
    return listenSock;
}

// Poll for data from clients in a given session. One thread for each session
void *pollSession(void *sessionPtr) {
    Session *session = (Session *)sessionPtr;
    fprintf(stderr, "In thread for session: %s\n", session->name);
    while (!session->sessionDeleted) {
        // TODO: Implement measures for when a client disconnects. Currently hangs server thread
        unsigned numTalking; 
        while (!(numTalking = session->numTalking))
            ; // TODO: Make it not spin
        // fprintf(stderr, "Polling session: %s. Num talking: %d\n", session->name, session->numTalking);
        int pollRet = poll(session->clientFds, numTalking, 8000);
        if (pollRet == 0){
            printf("timer timeout!\n");
            //exit
            continue;
        }
        fprintf(stderr, "Received data on session \"%s\" socket. Num clients: %d. Num Talking:%d\n", session->name, session->numClients, numTalking);
        for (int i = 0; i < numTalking; ++i) {
            if (session->clientFds[i].revents & POLLIN) { // Got data from this client
                char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
                int bytesRecv = recv(session->clientFds[i].fd, input, MAX_SOCKET_INPUT_SIZE, 0);
                input[bytesRecv] = '\0';
                fprintf(stderr, "Received data from talking client num: %d. Data: %s\n", i, input);
                message *msg = parseMessageAsString(input);
                if (msg->type == MESSAGE) {
                    unsigned strLength = 0;
                    strcpy(msg->session, session->name);
                    char *output = messageToString(msg, &strLength);
                    fprintf(stderr, "Broadcasting message: %s\n", output);
                    broadcastMessage(output, strLength, session, session->clientFds[i].fd);
                    sendResponse(MESSAGE_ACK, "", getClientByFd(session->clientFds[i].fd, session));
                }
                else {
                    performCommand(msg, session, getClientByFd(session->clientFds[i].fd, session));
                }
                free(input);
                free(msg);
            }
        }
    }
    // fprintf(stderr, "Freeing data and exiting thread for session: %s\n", session->name);
    // Session was deleted so we need to free session and its data
    free(session->name);
    free(session->thread); // Is this even legal?
    free(session);
    pthread_exit(0);
    return NULL;
}

// Session 0 (meta session) is for users who are not in a session and it only accepts commands
void *pollMetaSession(void *metaSessionPtr) {
    Session *metaSession = (Session *)metaSessionPtr;
    while (true) {
        lock(&sessions->metaSessionLock);
        unsigned numClients = metaSession->numClients;
        if (numClients == 0) { // This avoids spining while waiting for members
            fprintf(stderr, "Sleeping in meta session thread\n");
            wait(&sessions->metaSessionHasMembers, &sessions->metaSessionLock);
            fprintf(stderr, "Woke up in meta session thread\n");
        }
        unlock(&sessions->metaSessionLock);
        int pollRet = poll(metaSession->clientFds, numClients, 8000); // Blocks, may change to timeout
        if (pollRet == 0){
            printf("timer timeout!\n");
            continue;
        }
        fprintf(stderr, "Received data on meta session socket. Num clients: %d\n", metaSession->numClients);
        for (int i = 0; i < numClients; ++i) {
            if (metaSession->clientFds[i].revents & POLLIN) { // Got data from this client
                char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
                int bytesRecv = recv(metaSession->clientFds[i].fd, input, MAX_SOCKET_INPUT_SIZE, 0);
                input[bytesRecv] = '\0';
                fprintf(stderr, "Received data in meta session from client num: %d. Data: %s\n", i, input);
                message *msg = parseMessageAsString(input);
                // Check if they're trying to do a command while not logged in
                if (!metaSession->clients[i]->loggedIn && msg->type != LOGIN) {
                    char response[] = "You must log in first!";
                    sendResponse(MESSAGE_NACK, response, getClientByFd(metaSession->clientFds[i].fd, metaSession));
                }
                // Ensure that the client sent a command and not a message
                else if (msg->type == MESSAGE) {
                    char response[] = "You can't send messages until you create or join a session!";
                    sendResponse(MESSAGE_NACK, response, getClientByFd(metaSession->clientFds[i].fd, metaSession));
                }
                else {
                    performCommand(msg, metaSession, getClientByFd(metaSession->clientFds[i].fd, metaSession));
                }
                free(input);
                free(msg);
            }
        }
    }
    return NULL;
}

//==============================================//
// Serialization functions 
//==============================================//
// Converts string from socket to message type for analysis
message *parseMessageAsString(char *input) {
    char *type = strtok(input, ":");
    char *size = strtok(NULL, ":");
    char *session = strtok(NULL, ":");
    char *source = strtok(NULL, ":");
    char *data = strtok(NULL, "\0");
    // fprintf(stderr, "Type: %s. Size: %s. Session: %s. Source: %s Data: %s\n", type, size, session, source, (data ? data : ""));
    message *msg = malloc(sizeof(message));
    msg->type = atoi(type);
    msg->size = atoi(size);
    strcpy(msg->session, session);
    strcpy(msg->source, source);
    strncpy(msg->data, data, msg->size);
    msg->data[msg->size] = '\0';
    return msg;
}

// Convert message to string. Returns size in size parameter
char *messageToString(message *msg, unsigned *size) {
    int bytesPrinted = 0;
    // Allocate on heap a string with enough size for everything but data
    char *result = (char *)malloc(sizeof(char) * 100);
    // Print everything but data into the string
    bytesPrinted = snprintf(result, 100, "%d:%u:%s:%s:", msg->type, msg->size, msg->session, msg->source);
    // fprintf(stderr, "String without data: %s\nLength: %lu\n", result, strlen(result));
    // Resize string to have space to store data
    result = (char *)realloc(result, bytesPrinted + msg->size);
    // Inserts data where snprintf put '\0'
    memcpy(result + bytesPrinted, msg->data, msg->size);
    // fprintf(stderr, "String with data: ");
    *size = bytesPrinted + msg->size;
    return result;
}

//==============================================//
// Communication functions
//==============================================//
// Send message to all connected clients
void broadcastMessage(char *messageAsString, unsigned strLength, Session *session, int senderfd) {
    fprintf(stderr, "Sending message to %d clients in session: %s\n", session->numClients, session->name);
    for (int i = 0; i < session->numClients; ++i) {
        if (session->clients[i]->clientfd == senderfd)
            continue;
        send(session->clients[i]->clientfd, messageAsString, strLength, 0);
    }
    // fprintf(stderr, "Completed sending message in session: %s\n", session->name);
}

// Send message to specific client
void sendResponse(messageTypes type, char *response, Client *client) {
    message *msg = malloc(sizeof(message));
    msg->type = type;
    msg->size = strlen(response);
    strcpy(msg->session, "Server");
    strcpy(msg->source, "Server");
    if (strlen(response) >= 1)
        strcpy(msg->data, response);
    unsigned strLength;
    char *msgAsString = messageToString(msg, &strLength);
    send(client->clientfd, msgAsString, strLength, 0);
}

// Perform command given by a client
void performCommand(message *msg, Session *session, Client *client) {
    switch (msg->type) {
        case MESSAGE: {
            fprintf(stderr, "Error: Message of type message being sent to parse command!\n");
            break;
        }
        case LOGIN: {
            // TODO: Implement checking username and password
            if (!client->loggedIn) {
                // Free "Unknown" name currently in client
                free(client->name);
                // Save username
                client->name = malloc(sizeof(char) * strlen(msg->source));
                strcpy(client->name, msg->source);
                // Save password
                client->password = malloc(sizeof(char) * msg->size);
                strncpy(client->password, msg->data, msg->size);
                // Mark as logged in
                client->loggedIn = true;
                // Send login acknowledgement
                sendResponse(LOGIN_ACK, "", client);
            } 
            else {
                sendResponse(LOGIN_NACK, "Error: You are already logged in!", client);
            }
            break;
        }
        case LOGOUT: {
            if (client->loggedIn) {
                // Free username
                free(client->name);
                client->name = strdup("Unknown");
                // Free password
                free(client->password);
                client->password = NULL;
                // Mark as logged in
                client->loggedIn = false;
                removeClientFromSession(client, session);
                // Send login acknowledgement
                sendResponse(LOGOUT_ACK, "", client);
            }
            else { // Already logged out
                sendResponse(LOGOUT_NACK, "Error: You are not logged in!", client);
            }
            break;
        }
        case NEW_SESS: { ;
            // Check if session with that name already exists
            Session *searchSession = searchForSession(msg->data);
            if (searchSession != NULL) {
                sendResponse(NEW_SESS_NACK, "Session with that name already exists!", client);
                return;
            }
            fprintf(stderr, "Creating new session: %s\n", msg->data);
            Session *newSession = initSession(msg->data);
            addSessionToList(newSession);
            // TODO: Add error checking here
            addClientToSession(client, newSession);
            sendResponse(NEW_SESS_ACK, msg->data, client);
            newSession->thread = malloc(sizeof(pthread_t));
            pthread_create((pthread_t *) newSession->thread, NULL, (void *) pollSession, (void *) newSession);
            break; 
        }
        case JOIN_SESS: { ;
            Session *searchSession = searchForSession(msg->data);
            if (searchSession == NULL) {
                fprintf(stderr, "Session named: \"%s\" does not exist\n", msg->data);
                sendResponse(JOIN_SESS_NACK, "Session with that name does not exist!", client);
                return;
            }
            // TODO: Add error checking here
            addClientToSession(client, searchSession);
            sendResponse(JOIN_SESS_ACK, msg->data, client);
            assert(searchSession->thread != NULL);
            break; 
        }
        case LEAVE_SESS: {
            if (client->numSessions == 0) { // Only in the meta session
                sendResponse(LEAVE_SESS_NACK, "You're not in a session to leave!", client);
                return;
            }
            // Removes client from the session and deletes session if it is empty
            removeClientFromSession(client, session);
            sendResponse(LEAVE_SESS_ACK, "", client);
            if (client->talkingToSession != sessions->frontSession) { // Inform client which session theyve switched into
                // unsigned responseLen = snprintf(NULL, 0, "Switched to talking in session: %s", client->talkingToSession->name); 
                // char response[responseLen + 1];
                // sprintf(response, "Switched to talking in session: %s", client->talkingToSession->name);
                sendResponse(SWITCH_SESS, client->talkingToSession->name, client);
            }
            break;
        }
        case QUERY: { ;
            char *result = malloc(sizeof(char) * 2048);
            memset(result, '\0', 2048);
            strcat(result, "\%========================================\%\n");
            Session *sessionData;
            for (sessionData = sessions->frontSession; sessionData != NULL; sessionData = sessionData->nextSession) {
                if (sessionData->numClients == 0)
                    continue;
                // fprintf(stderr, "Printing query for session: %s. Num Clients: %d\n", sessionData->name, sessionData->numClients);
                // Variables to store partially done string
                int strLength = 0; 
                char sessionInfo[512];
                // Change format when listing users that are not in a session
                if (sessionData == sessions->frontSession)
                    strLength += sprintf(sessionInfo, "Users not in a session:\n");
                else
                    strLength += sprintf(sessionInfo, "Session: %s\nUsers: \n", sessionData->name);
                // Add names of users in sessions on new lines
                for (int j = 0; j < sessionData->numClients; ++j) {
                    strLength += sprintf(sessionInfo + strLength, "%s\n", sessionData->clients[j]->name);
                }
                strcat(result, sessionInfo);
            }
            strcat(result, "\%========================================\%\n");
            sendResponse(QUERY_ACK, result, client);
            free(result);
            break; 
        }
        case EXIT:
            // TODO
            break;
        default:
            break;
    }
}