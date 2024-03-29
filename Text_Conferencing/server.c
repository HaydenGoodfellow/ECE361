#include "server.h"

// Parts of this code was adapted from Beej's Guide to Network Programming
// Found online at: https://beej.us/guide/bgnet/html/

session sList[64]; // Session List
unsigned numSessions = 1;

int main(int argc, char **argv) {
	if (argc < 2 || (atoi(argv[1]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using \"server <TCP Listen Port>\"\n");
        return 0;
    }
    char *portNum = argv[1];
    struct addrinfo hints, *serverInfo, *ptr;
    struct sockaddr client;
    socklen_t clientAddrLen;  
    // Set up hints struct
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET; // Ensures IPv4
    hints.ai_socktype = SOCK_STREAM; // Ensures TCP
    // Get address info
    int getAddrReturn = getaddrinfo(NULL, portNum, &hints, &serverInfo);
    if (getAddrReturn != 0) {
        fprintf(stderr, "Failed to get address info: %s\n", gai_strerror(getAddrReturn));
        return 0;
    }
    // Set up socket
    fprintf(stderr, "Setting up socket\n");
    int listenSock = socket(AF_INET /*IPv4*/, SOCK_STREAM /*TCP*/, 0);
    if (listenSock < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 0;
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
    sList[0].sessionName = malloc(sizeof(char) * strlen("No Session"));
    strcpy(sList[0].sessionName, "No Session");
    // Set up fd we will poll and accept other fds from
    struct pollfd fds[1];
    memset(fds, 0, sizeof(fds));
    fds[0].fd = listenSock;
    fds[0].events = POLLIN;
    // Session 0 is for users who are not in a session
    volatile int threadForMetaSession = 0;
    // Wait for new clients to connect to the socket
    while (true) {
        int pollRet = poll(fds, 1, -1);
        if (pollRet == 0)
            continue;
        assert(fds[0].revents & POLLIN); // Ensure read ready
        fprintf(stderr, "Received connection on listener socket\n");
        clientAddrLen = sizeof(client);
        int clientfd = accept(listenSock, (struct sockaddr *)&client, (socklen_t *)&clientAddrLen);

        sList[0].clientFds[sList[0].numClients].fd = clientfd;
        sList[0].clientFds[sList[0].numClients].events = POLLIN;
        sList[0].numClients++;
        fprintf(stderr, "New client fd: %d, Num clients: %d\n", clientfd, sList[0].numClients);
        if (!threadForMetaSession) {
            fprintf(stderr, "Creating thread\n");
            sList[0].thread = malloc(sizeof(pthread_t));
            pthread_create((pthread_t *) sList[0].thread, NULL, (void *) pollMetaSession, NULL);
            threadForMetaSession = 1;
        }
    }
    close(listenSock);
    return 0;
}

// Session 0 (meta session) is for users who are not in a session and it only accepts commands
void *pollMetaSession() {
    while (true) {
        int pollRet = poll(sList[0].clientFds, sList[0].numClients, 500); // Blocks, may change to timeout
        if (pollRet == 0)
            continue;
        fprintf(stderr, "Received data on session 0 socket. Num clients: %d\n", sList[0].numClients);
        for (int i = 0; i < sList[0].numClients; ++i) {
            if (sList[0].clientFds[i].revents & POLLIN) {
                char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
                recv(sList[0].clientFds[i].fd, input, MAX_SOCKET_INPUT_SIZE, 0);
                fprintf(stderr, "Received data from client: %d data: %s\n", i, input);
                message *msg = parseMessageAsString(input);
                if (msg->type == MESSAGE) {
                    sendResponse(MESSAGE_NACK, "You can't send messages until you create or join a session!\n", 0, i);
                    continue;
                }
                else {
                    performCommand(msg, 0, i);
                }
            }
        }
    }
    return NULL;
}

void *pollSession(void *sessionNum /*Session number*/) {
    unsigned sNum = *((int *)sessionNum);
    fprintf(stderr, "In thread for session: %d\n", sNum);
    while (true) {
        int pollRet = poll(sList[sNum].clientFds, sList[sNum].numClients, 500); // Blocks, may change to timeout
        if (pollRet == 0)
            continue;
        fprintf(stderr, "Received data on session: %d socket. Num clients: %d\n", sNum, sList[sNum].numClients);
        for (int i = 0; i < sList[sNum].numClients; ++i) {
            if (sList[sNum].clientFds[i].revents & POLLIN) {
                char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
                recv(sList[sNum].clientFds[i].fd, input, MAX_SOCKET_INPUT_SIZE, 0);
                fprintf(stderr, "Received data from client: %d data: %s\n", i, input);
                message *msg = parseMessageAsString(input);
                if (msg->type == MESSAGE) {
                    unsigned strLength = 0;
                    char *output = messageToString(msg, &strLength);
                    fprintf(stderr, "Broadcasting message: %s\n", output);
                    broadcastMessage(output, strLength, sNum, i);
                }
                else {
                    performCommand(msg, sNum, i);
                }
            }
        }
    }
    return NULL;
}

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

void broadcastMessage(char *messageAsString, unsigned strLength, unsigned sNum, unsigned clientNum) {
    fprintf(stderr, "Sending message: %s\n", messageAsString);
    for (int i = 0; i < sList[sNum].numClients; ++i) {
        if (i == clientNum)
            continue;
        send(sList[sNum].clientFds[i].fd, messageAsString, strLength, 0);
    }
}

void performCommand(message *msg, unsigned sNum, unsigned clientNum) {
    switch (msg->type) {
        case MESSAGE:
            fprintf(stderr, "Message of type message being sent to parse command");
            break;
        case LOGIN:
            if (!sList[sNum].clients[clientNum].loggedIn) { // Not logged in
                // Save username
                sList[sNum].clients[clientNum].name = malloc(sizeof(char) * strlen(msg->source));
                strcpy(sList[sNum].clients[clientNum].name, msg->source);
                // Save password
                sList[sNum].clients[clientNum].password = malloc(sizeof(char) * msg->size);
                strncpy(sList[sNum].clients[clientNum].password, msg->data, msg->size);
                // Mark as logged in
                sList[sNum].clients[clientNum].loggedIn = true;
                // Send login acknowledgement
                sendResponse(LOGIN_ACK, "", sNum, clientNum);
            }
            else { // Already logged in
                sendResponse(LOGIN_NACK, "Error: You are already logged in!", sNum, clientNum);
            }
            break;
        case LOGOUT:
            if (sList[sNum].clients[clientNum].loggedIn) { // Logged in
                // Free username
                free(sList[sNum].clients[clientNum].name);
                sList[sNum].clients[clientNum].name = NULL;
                // Free password
                free(sList[sNum].clients[clientNum].password);
                sList[sNum].clients[clientNum].password = NULL;
                // Mark as logged in
                sList[sNum].clients[clientNum].loggedIn = false;
                removeClientFromSession(sNum, clientNum);
                // Send login acknowledgement
                sendResponse(LOGOUT_ACK, "", sNum, clientNum);
            }
            else { // Already logged out
                sendResponse(LOGOUT_NACK, "Error: You are not logged in!", sNum, clientNum);
            }
            break;
        case NEW_SESS:
            fprintf(stderr, "Creating new session: %s\n", msg->data);
            if (numSessions < 63 && sNum == 0) {
                sList[numSessions].sessionName = malloc(sizeof(char) * msg->size);
                strcpy(sList[numSessions].sessionName, msg->data);
                sList[numSessions].sessionNum = numSessions;
                sList[numSessions].clientFds[0].fd = sList[0].clientFds[clientNum].fd;
                sList[numSessions].clientFds[0].events = POLLIN;
                sList[numSessions].clients[0].name = sList[0].clients[clientNum].name;
                sList[numSessions].clients[0].password = sList[0].clients[clientNum].password;
                sendResponse(NEW_SESS_ACK, "", sNum, clientNum);
                removeClientFromSession(0, clientNum);
                --sList[0].numClients;
                ++sList[numSessions].numClients;
                sList[numSessions].thread = malloc(sizeof(pthread_t));
                unsigned sessionNumCopy = numSessions;
                pthread_create((pthread_t *) sList[numSessions].thread, NULL, (void *) pollSession, (void *) &sessionNumCopy);
                ++numSessions;
            }
            else if (sNum != 0) {
                sendResponse(NEW_SESS_NACK, "Can't create new session while in a session!", sNum, clientNum);
            }
            else {
                sendResponse(NEW_SESS_NACK, "Too many sessions already exist!", sNum, clientNum);
            }
            break;
        case JOIN_SESS:
            if (sNum != 0) {
                sendResponse(JOIN_SESS_NACK, "Can't join another session while in a session!", sNum, clientNum);
                return;
            }
            unsigned sessionNum = 0;
            for (int i = 1; i < numSessions; ++i) {
                if (strcmp(sList[i].sessionName, msg->data) == 0) {
                    sessionNum = i;
                    break;
                }
            }
            if (sessionNum == 0) {
                sendResponse(JOIN_SESS_NACK, "Session with that name not found!", sNum, clientNum);
                return;
            }
            unsigned numClients = sList[sessionNum].numClients;
            sList[sessionNum].clientFds[numClients].fd = sList[0].clientFds[clientNum].fd;
            sList[sessionNum].clientFds[numClients].events = POLLIN;
            sList[sessionNum].clients[numClients].name = sList[0].clients[clientNum].name;
            sList[sessionNum].clients[numClients].password = sList[0].clients[clientNum].password;
            sendResponse(JOIN_SESS_ACK, "", sNum, clientNum);
            removeClientFromSession(0, clientNum);
            --sList[0].numClients;
            ++sList[sessionNum].numClients;
            break;
        case LEAVE_SESS:
            if (sNum == 0) {
                sendResponse(LEAVE_SESS_NACK, "You're not in a session to leave!", sNum, clientNum);
                return;
            }
            numClients = sList[0].numClients;
            sList[0].clientFds[numClients].fd = sList[sNum].clientFds[clientNum].fd;
            sList[0].clientFds[numClients].events = POLLIN;
            sList[0].clients[numClients].name = sList[sNum].clients[clientNum].name;
            sList[0].clients[numClients].password = sList[sNum].clients[clientNum].password;
            sendResponse(LEAVE_SESS_ACK, "", sNum, clientNum);
            removeClientFromSession(sNum, clientNum);
            ++sList[0].numClients;
            --sList[sNum].numClients;
            break;
        case QUERY: ;
            char result[2048];
            for (int i = 0; i < numSessions; ++i) {
                int strLength = 0; 
                char sessionInfo[512];
                strLength += sprintf(sessionInfo, "Session: %s\nUsers: \n", sList[i].sessionName);
                for (int j = 0; j < sList[i].numClients; ++j) {
                    strLength += sprintf(sessionInfo + strLength, "%s\n", sList[i].clients[j].name);
                }
                strcat(result, sessionInfo);
            }
            sendResponse(QUERY_ACK, result, sNum, clientNum);
            break;
        case EXIT:
            removeClientFromSession(sNum, clientNum);
            break;
        default:
            break;
    }
}

void sendResponse(messageTypes type, char *response, unsigned sNum, unsigned clientNum) {
    message *msg = malloc(sizeof(message));
    msg->type = type;
    msg->size = strlen(response);
    strcpy(msg->source, "Server");
    strcpy(msg->data, response);
    unsigned strLength;
    char *msgAsString = messageToString(msg, &strLength);
    send(sList[sNum].clientFds[clientNum].fd, msgAsString, strLength, 0);
}

void removeClientFromSession(unsigned sNum, unsigned clientNum) {
    unsigned numClients = sList[sNum].numClients;
    for (unsigned i = clientNum; i < numClients - 1; ++i) {
        sList[sNum].clientFds[i].fd = sList[sNum].clientFds[i + 1].fd;
        sList[sNum].clientFds[i].events = sList[sNum].clientFds[i + 1].events;
        sList[sNum].clients[i].name = sList[sNum].clients[i + 1].name;
        sList[sNum].clients[i].password = sList[sNum].clients[i + 1].password;
    }
}