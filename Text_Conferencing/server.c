#include "server.h"

// Parts of this code was adapted from Beej's Guide to Network Programming
// Found online at: https://beej.us/guide/bgnet/html/

session sList[64]; // Session List


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
        int pollRet = poll(sList[0].clientFds, sList[0].numClients, -1); // Blocks, may change to timeout
        if (pollRet == 0)
            continue;
        fprintf(stderr, "Received data on session 0 socket. Num clients: %d\n", sList[0].numClients);
        for (int i = 0; i < sList[0].numClients; ++i) {
            if (sList[0].clientFds[i].revents & POLLIN) {
                fprintf(stderr, "Received data from client: %d\n", i);
                char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
                int numBytes = recv(sList[sNum].clientFds[i].fd, input, MAX_SOCKET_INPUT_SIZE, 0);
                fprintf(stderr, "Received data: %s\n", input);
                message *msg = parseMessageAsString(input);
                if (msg->type == MESSAGE) {
                    sendResponse("You can't send messages until you create or join a session!\n", 0, i);
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

void *pollSession(unsigned sNum /*Session number*/) {
    fprintf(stderr, "In thread for session: %d\n", sNum);
    while (true) {
        int pollRet = poll(sList[sNum].clientFds, sList[sNum].numClients, 500); // Blocks, may change to timeout
        if (pollRet == 0)
            continue;
        fprintf(stderr, "Received data on session: %d socket. Num clients: %d\n", sNum, sList[sNum].numClients);
        for (int i = 0; i < sList[sNum].numClients; ++i) {
            if (sList[sNum].clientFds[i].revents & POLLIN) {
                fprintf(stderr, "Received data from client: %d\n", i);
                char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
                int numBytes = recv(sList[sNum].clientFds[i].fd, input, MAX_SOCKET_INPUT_SIZE, 0);
                fprintf(stderr, "Received data: %s\n", input);
                for (int j = 0; j < sList[sNum].numClients; ++j) {
                    if (i == j)
                        continue;
                    fprintf(stderr, "Sending data: %s\n", input);
                    send(sList[sNum].clientFds[j].fd, input, numBytes, 0);
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
    unsigned dataIndex = 4 + strlen(type) + strlen(size) + strlen(source);
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
            fprintf(stderr, "Message of type message being sent to parse command\n");
            break;
        case LOGIN:
            if (!sList[sNum].clients[clientNum].loggedIn) { // Not logged in
                // Save username
                sList[sNum].clients[clientNum].name = malloc(sizeof(char) * strlen(msg->source));
                strcpy(sList[sNum].clients[clientNum].name, msg->source);
                // Save password
                sList[sNum].clients[clientNum].password = malloc(sizeof(char) * msg->size);
                strncpy(sList[sNum].clients[clientNum].name, msg->data, msg->size);
                // Mark as logged in
                sList[sNum].clients[clientNum].loggedIn = true;
            }
            else { // Already logged in
                
            }
            break;
        case LOGOUT:
            break;
        case NEW_SESS:
            break;
        case JOIN_SESS:
            break;
        case LEAVE_SESS:
            break;
        case QUERY:
            break;
        case EXIT:
            break;
        default:
            break;
    }
}

void sendResponse(char *response, unsigned sNum, unsigned clientNum) {

}