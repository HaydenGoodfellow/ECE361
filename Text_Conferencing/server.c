#include "server.h"

// Parts of this code was adapted from Beej's Guide to Network Programming
// Found online at: https://beej.us/guide/bgnet/html/

// Rename thread functions to simple names for readability
#define lock(x) pthread_mutex_lock(x) 
#define unlock(x) pthread_mutex_unlock(x) 
#define wait(x, y) pthread_cond_wait(x, y) 
#define signal(x) pthread_cond_signal(x) 
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
    fprintf(stderr, "In main1\n");
    char *portNum = argv[1];
    // Create socket and make it a listening socket
    int listenSock = establishConnection(portNum);

    // Allocate heap memory for client list and session list
    clients = malloc(sizeof(ClientList));
    assert(clients != NULL);
    int ret = pthread_mutex_init(&clients->clientListLock, NULL);
    assert(ret == 0);
    sessions = malloc(sizeof(SessionList));
    assert(sessions != NULL);
    ret = pthread_mutex_init(&sessions->sessionListLock, NULL);
    assert(ret == 0);

    // Create meta session for people not in a session (Doesn't allow talking, only commands)
    Session *metaSession = initSession("Not in a Session");
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
    while (true) {
        unsigned numClients = session->numClients;
        int pollRet = poll(session->clientFds, numClients, 500); // Blocks, may change to timeout
        if (pollRet == 0)
            continue;
        fprintf(stderr, "Received data on meta session socket. Num clients: %d\n", numClients);
        for (int i = 0; i < numClients; ++i) {
            if (session->clientFds[i].revents & POLLIN) { // Got data from this client
                char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
                recv(session->clientFds[i].fd, input, MAX_SOCKET_INPUT_SIZE, 0);
                fprintf(stderr, "Received data from client: %d data: %s\n", i, input);
                message *msg = parseMessageAsString(input);
                if (msg->type == MESSAGE) {
                    unsigned strLength = 0;
                    char *output = messageToString(msg, &strLength);
                    fprintf(stderr, "Broadcasting message: %s\n", output);
                    broadcastMessage(output, strLength, session, i);
                }
                else {
                    performCommand(msg, session, session->clients[i]);
                }
            }
        }
    }
    return NULL;
}

// Session 0 (meta session) is for users who are not in a session and it only accepts commands
void *pollMetaSession(void *metaSessionPtr) {
    Session *metaSession = (Session *)metaSessionPtr;
    while (true) {
        unsigned numClients = metaSession->numClients;
        int pollRet = poll(metaSession->clientFds, numClients, 500); // Blocks, may change to timeout
        if (pollRet == 0)
            continue;
        fprintf(stderr, "Received data on meta session socket. Num clients: %d\n", numClients);
        for (int i = 0; i < numClients; ++i) {
            if (metaSession->clientFds[i].revents & POLLIN) { // Got data from this client
                char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
                recv(metaSession->clientFds[i].fd, input, MAX_SOCKET_INPUT_SIZE, 0);
                fprintf(stderr, "Received data from client: %d data: %s\n", i, input);
                message *msg = parseMessageAsString(input);
                // Check if they're trying to do a command while not logged in
                if (!metaSession->clients[i]->loggedIn && msg->type != LOGIN) {
                    char response[] = "You must log in first!\n";
                    sendResponse(MESSAGE_NACK, response, metaSession->clients[i]);
                }
                // Ensure that the client sent a command and not a message
                else if (msg->type == MESSAGE) {
                    char response[] = "You can't send messages until you create or join a session!\n";
                    sendResponse(MESSAGE_NACK, response, metaSession->clients[i]);
                }
                else {
                    performCommand(msg, metaSession, metaSession->clients[i]);
                }
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
    char *source = strtok(NULL, ":");
    unsigned dataIndex = 3 + strlen(type) + strlen(size) + strlen(source);
    message *msg = malloc(sizeof(message));
    msg->type = atoi(type);
    msg->size = atoi(size);
    strcpy(msg->source, source);
    strncpy(msg->data, input + dataIndex, msg->size);
    return msg;
}

// Convert message to string. Returns size in size parameter
char *messageToString(message *msg, unsigned *size) {
    int bytesPrinted = 0;
    // Allocate on heap a string with enough size for everything but data
    char *result = (char *)malloc(sizeof(char) * 100);
    // Print everything but data into the string
    bytesPrinted = snprintf(result, 100, "%d:%u:%s:", msg->type, msg->size, msg->source);
    // fprintf(stderr, "String without data: %s\nLength: %lu\n", result, strlen(result));
    // Resize string to have space to store data
    result = (char *)realloc(result, bytesPrinted + msg->size);
    // Inserts data where snprintf put '\0'
    memcpy(result + bytesPrinted, msg->data, msg->size);
    // fprintf(stderr, "String with data: ");
    // for (int i = 0; i < bytesPrinted + msg->size; ++i)
    //     fprintf(stderr, "%c", result[i]);
    // fprintf(stderr, "\nLength: %d\n", bytesPrinted + msg->size);
    *size = bytesPrinted + msg->size;
    return result;
}

//==============================================//
// Communication functions
//==============================================//
// Send message to all connected clients
void broadcastMessage(char *messageAsString, unsigned strLength, Session *session, int sender) {
    fprintf(stderr, "Sending message: %s\n", messageAsString);
    for (int i = 0; i < session->numClients; ++i) {
        if (i == sender)
            continue;
        send(session->clientFds[i].fd, messageAsString, strLength, 0);
    }
}

// Send message to specific client
void sendResponse(messageTypes type, char *response, Client *client) {
    message *msg = malloc(sizeof(message));
    msg->type = type;
    msg->size = strlen(response);
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
        case MESSAGE:
            fprintf(stderr, "Error: Message of type message being sent to parse command!\n");
            break;
        case LOGIN:
            // TODO: Implement checking username and password
            // TODO: Solve problem when client is in multiple sessions so it does command multiple times
            if (!client->loggedIn) {
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
        case LOGOUT:
            if (client->loggedIn) {
                // Free username
                free(client->name);
                client->name = NULL;
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
        case NEW_SESS: ;
            // Check if session with that name already exists
            Session *searchSession = searchForSession(msg->data);
            if (searchSession != NULL) {
                sendResponse(NEW_SESS_NACK, "Session with that name already exists!", client);
                return;
            }
            fprintf(stderr, "Creating new session: %s\n", msg->data);
            Session *newSession = initSession(msg->data);
            addSessionToList(newSession);
            // TODO: Adder error checking here
            addClientToSession(client, session);
            sendResponse(NEW_SESS_ACK, "", client);
            newSession->thread = malloc(sizeof(pthread_t));
            pthread_create((pthread_t *) newSession->thread, NULL, (void *) pollSession, (void *) newSession);
            break;
        case JOIN_SESS:
            // if (sNum != 0) {
            //     sendResponse(JOIN_SESS_NACK, "Can't join another session while in a session!", sNum, clientNum);
            //     return;
            // }
            // unsigned sessionNum = 0;
            // for (int i = 1; i < numSessions; ++i) {
            //     if (strcmp(sList[i].sessionName, msg->data) == 0) {
            //         sessionNum = i;
            //         break;
            //     }
            // }
            // if (sessionNum == 0) {
            //     sendResponse(JOIN_SESS_NACK, "Session with that name not found!", sNum, clientNum);
            //     return;
            // }
            // unsigned numClients = sList[sessionNum].numClients;
            // sList[sessionNum].clientFds[numClients].fd = sList[0].clientFds[clientNum].fd;
            // sList[sessionNum].clientFds[numClients].events = POLLIN;
            // sList[sessionNum].clients[numClients].name = sList[0].clients[clientNum].name;
            // sList[sessionNum].clients[numClients].password = sList[0].clients[clientNum].password;
            // sendResponse(JOIN_SESS_ACK, "", sNum, clientNum);
            // removeClientFromSession(0, clientNum);
            // --sList[0].numClients;
            // ++sList[sessionNum].numClients;
            break;
        case LEAVE_SESS:
            // if (sNum == 0) {
            //     sendResponse(LEAVE_SESS_NACK, "You're not in a session to leave!", sNum, clientNum);
            //     return;
            // }
            // numClients = sList[0].numClients;
            // sList[0].clientFds[numClients].fd = sList[sNum].clientFds[clientNum].fd;
            // sList[0].clientFds[numClients].events = POLLIN;
            // sList[0].clients[numClients].name = sList[sNum].clients[clientNum].name;
            // sList[0].clients[numClients].password = sList[sNum].clients[clientNum].password;
            // sendResponse(LEAVE_SESS_ACK, "", sNum, clientNum);
            // removeClientFromSession(sNum, clientNum);
            // ++sList[0].numClients;
            // --sList[sNum].numClients;
            break;
        case QUERY: ;
            // char result[2048];
            // for (int i = 0; i < numSessions; ++i) {
            //     int strLength = 0; 
            //     char sessionInfo[512];
            //     strLength += sprintf(sessionInfo, "Session: %s\nUsers: \n", sList[i].sessionName);
            //     for (int j = 0; j < sList[i].numClients; ++j) {
            //         strLength += sprintf(sessionInfo + strLength, "%s\n", sList[i].clients[j].name);
            //     }
            //     strcat(result, sessionInfo);
            // }
            // sendResponse(QUERY_ACK, result, sNum, clientNum);
            break;
        case EXIT:
            // removeClientFromSession(sNum, clientNum);
            break;
        default:
            break;
    }
}