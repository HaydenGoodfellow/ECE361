#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
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

#define MAX_USER_INPUT_SIZE 2048
#define MAX_SOCKET_INPUT_SIZE 2048

void *getResponse(void *sockfd);

message *parseInput(char *input);

char *messageToString(message *msg, unsigned *size);
message *parseMessageAsString(char *input);
void evaluateResponse(message* msg);