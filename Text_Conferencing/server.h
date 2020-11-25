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

typedef struct clientData {
    char *name;
    char *password; // Super not secure, only doing this for project
    bool loggedIn;
} clientData;

typedef struct session {
    pthread_t* thread;
    unsigned sessionNum;
    struct pollfd clientFds[64]; // Max 64 clients
    unsigned numClients;
    clientData clients[64]; // Max 64 clients
} session;

// Session 0 (meta session) is for users who are not in a session and it only accepts commands
void *pollMetaSession();

void *pollSession(unsigned sessionNum);

message *parseMessageAsString(char *input);

char *messageToString(message *msg, unsigned *size);

void broadcastMessage(message *msg, unsigned sNum, unsigned clientNum);

void performCommand(message *msg, unsigned sNum, unsigned clientNum);

void sendResponse(char *response, unsigned sNum, unsigned clientNum);