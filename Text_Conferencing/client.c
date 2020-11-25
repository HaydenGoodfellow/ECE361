#include "client.h"

// Parts of this code was adapted from Beej's Guide to Network Programming
// Found online at: https://beej.us/guide/bgnet/html/

int main(int argc, char **argv) {
    // Get user to log in
    bool validInput = false;
    char userLogin[MAX_USER_INPUT_SIZE];
    char *clientID = NULL;
    char *password = NULL; // Super insecure, only doing this for project
    char *serverIP = NULL;
    char *port = NULL;
    while (!validInput) {
        fprintf(stderr, "Please log in using \"/login <Client ID> <Password> <Server IP> <Server Port>\"\n");
        fgets(userLogin, MAX_USER_INPUT_SIZE, stdin);
        // Strip newline character
        int length = strlen(userLogin);
        if ((length > 0) && (userLogin[length - 1] == '\n')) {
            userLogin[length - 1] = '\0';
            --length;
        }
        // Check if command is valid
        if (strncmp(userLogin, "/login ", 7) != 0) {
            fprintf(stderr, "Invalid input, please try again\n");
            continue;
        }
        // Make copy of login so we can use it later
        char *loginCopy = malloc(sizeof(char) * (strlen(userLogin) + 1));
        strcpy(loginCopy, userLogin);
        char *command = strtok(loginCopy, " ");
        clientID = strtok(NULL, " ");
        password = strtok(NULL, " ");
        serverIP = strtok(NULL, " ");
        port = strtok(NULL, " ");
        if (!command || !clientID || !password || !serverIP || !port) {
            fprintf(stderr, "Invalid input, please try again\n");
            continue;
        }
        validInput = true;
    }
    // Connect to server
    struct addrinfo hints, *serverInfo, *ptr;
    // Set up hints struct
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Ensures IPv4
    hints.ai_socktype = SOCK_STREAM; // Ensures TCP
    // Get address info
    
    int getAddrReturn = getaddrinfo(serverIP, port, &hints, &serverInfo);
    if (getAddrReturn != 0) {
        fprintf(stderr, "Failed to get address info: %s\n", gai_strerror(getAddrReturn));
        return 0;
    }
    int sockfd;
    // Open socket and connect
    for (ptr = serverInfo; ptr != NULL; ptr = ptr->ai_next) {
        sockfd = socket(AF_INET /*IPv4*/, SOCK_STREAM /*TCP*/, 0);
        if (sockfd < 0) {
            fprintf(stderr, "Failed to create socket\n");
            continue;
        }
        int connectRet = connect(sockfd, ptr->ai_addr, ptr->ai_addrlen);
        if (connectRet < 0) {
            fprintf(stderr, "Failed to connect\n");
            close(sockfd);
            continue;
        }
        break;
    }
    if (ptr == NULL) // Wasn't able to bind socket so exit program
        return 0;
    // Make thread to capture responses
    pthread_t *responseThread = malloc(sizeof(pthread_t));
    pthread_create(responseThread, NULL, (void *) getResponse, (void *) &sockfd);
    // Login to server
    message *msg = parseInput(userLogin);
    unsigned strLength = 0;
    char *msgAsString = messageToString(msg, &strLength);
    sendto(sockfd, msgAsString, strLength, MSG_CONFIRM, serverInfo->ai_addr, serverInfo->ai_addrlen);
    // Send user input to server
    char userInput[MAX_USER_INPUT_SIZE];
    while (true) {
        // Get user input
        fgets(userInput, MAX_USER_INPUT_SIZE, stdin);
        // Strip newline character
        int length = strlen(userInput);
        if ((length > 0) && (userInput[length - 1] == '\n')) {
            userInput[length - 1] = '\0';
            --length;
        }
        // Convert to message type and make string
        msg = parseInput(userInput);
        strLength = 0;
        msgAsString = messageToString(msg, &strLength);
        // Send message to server
        // fprintf(stderr, "Sending data: %s\n", msgAsString);
        sendto(sockfd, msgAsString, strLength, MSG_CONFIRM, serverInfo->ai_addr, serverInfo->ai_addrlen);
    }
    close(sockfd);
    return 0;
}

void *getResponse(void *sockfd) {
    char response[MAX_SOCKET_INPUT_SIZE];
    while (true) {
        // Receive message back from server
        int bytesRecv = recv(*((int *)sockfd), response, MAX_SOCKET_INPUT_SIZE, 0);
        response[bytesRecv] = '\0';
        message *msg = parseMessageAsString(response);
        evaluateResponse(msg);
    }
}

message *parseInput(char *input) {
    static char username[64];
    // Check if input is null
    if (input == NULL) {
        fprintf(stderr, "Invalid input!\n");
        return NULL;
    }
    message *msg = malloc(sizeof(message));
    if (strncmp(input, "/", 1) == 0) { // Command
        if (strncmp(input + 1, "login ", 6) == 0) {
            char *command = strtok(input, " ");
            char *clientID = strtok(NULL, " ");
            char *password = strtok(NULL, " ");
            char *serverIP = strtok(NULL, " ");
            char *port = strtok(NULL, " ");
            if (!command || !clientID || !password || !serverIP || !port) {
                fprintf(stderr, "Invalid login string\n");
                return NULL;
            }
            fprintf(stderr, "User name: %s, Password: %s\n", clientID, password);
            strcpy(username, clientID);
            msg->type = LOGIN;
            msg->size = strlen(password);
            strcpy(msg->source, username);
            strcpy(msg->data, password);
            return msg;
        }
        else if (strncmp(input + 1, "logout", 6) == 0) {
            msg->type = LOGOUT;
            msg->size = 0;
            strcpy(msg->source, username);
            return msg;
        }
        else if (strncmp(input + 1, "createsession ", 14) == 0) {
            char *command = strtok(input, " ");
            char *sessionID = strtok(NULL, " ");
            if (!command || !sessionID) {
                fprintf(stderr, "Invalid join session string\n");
                return NULL;
            }
            msg->type = NEW_SESS;
            msg->size = strlen(sessionID);
            strcpy(msg->source, username);
            strcpy(msg->data, sessionID);
            return msg;
        }
        else if (strncmp(input + 1, "joinsession ", 12) == 0) {
            char *command = strtok(input, " ");
            char *sessionID = strtok(NULL, " ");
            if (!command || !sessionID) {
                fprintf(stderr, "Invalid join session string\n");
                return NULL;
            }
            msg->type = JOIN_SESS;
            msg->size = strlen(sessionID);
            strcpy(msg->source, username);
            strcpy(msg->data, sessionID);
            return msg;
        }
        else if (strncmp(input + 1, "leavesession", 12) == 0) {
            msg->type = LEAVE_SESS;
            msg->size = 0;
            strcpy(msg->source, username);
            return msg;
        }
        else if (strncmp(input + 1, "list", 4) == 0) {
            msg->type = QUERY;
            msg->size = 0;
            strcpy(msg->source, username);
            return msg;
        }
        else if (strncmp(input + 1, "quit", 4) == 0) {
            exit(0);
        }
    }
    else  { // Message
        msg->type = MESSAGE;
        msg->size = strlen(input);
        strcpy(msg->source, username);
        strcpy(msg->data, input);
        return msg;
    }
    return NULL;
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

void evaluateResponse(message* msg) {
    // fprintf(stderr, "Evaluating response\n");
    switch (msg->type) {
        case MESSAGE_NACK:
            fprintf(stderr, "%s\n", msg->data);
            break;
        case LOGIN_ACK:
            fprintf(stderr, "Login successful.\n");
            break;
        case LOGIN_NACK:
            fprintf(stderr, "%s\n", msg->data);
            break;
        case LOGOUT_ACK:
            fprintf(stderr, "Logout successful.\n");
            break;
        case LOGOUT_NACK:
            fprintf(stderr, "%s\n", msg->data);
            break;
        case NEW_SESS_ACK:
            fprintf(stderr, "Session created successfully.\n");
            break;
        case JOIN_SESS_ACK:
            fprintf(stderr, "Join session successful.\n");
            break;
        case JOIN_SESS_NACK:
            fprintf(stderr, "%s\n", msg->data);
            break;
        case QUERY_ACK:
            fprintf(stderr, "%s\n", msg->data);
            break;
        case LEAVE_SESS_ACK:
            fprintf(stderr, "Leave session successful.\n");
            break;
        case LEAVE_SESS_NACK:
            fprintf(stderr, "%s\n", msg->data);
            break;
        default:
            break;
    }
}