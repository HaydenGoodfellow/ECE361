#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>

#define MAX_SOCKET_INPUT_SIZE 128 // Bytes

int main(int argc, char **argv) {
	if (argc < 2 || (atoi(argv[1]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using \"server <UDP Listen Port>\"\n");
        return 0;
    }
    int portNum = atoi(argv[1]);
    struct sockaddr_in server, client;
    // Set up server struct
    memset(&server, 0, sizeof(server));
    server.sin_addr.s_addr = INADDR_ANY; // Need to change
    server.sin_family = AF_INET;
    server.sin_port = htons(portNum);
    // Set up socket
    int sockfd = socket(AF_INET /*IPv4*/, SOCK_DGRAM /*UDP*/, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 0;
    }
    // Bind socket
    int bindReturn = bind(sockfd, (struct sockaddr *)&server, sizeof(server));
    if (bindReturn < 0) {
        fprintf(stderr, "Failed to bind socket\n");
        return 0;
    }
    // Wait for input to the socket
    char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
    socklen_t clientAddrLen = sizeof(struct sockaddr);
    recvfrom(sockfd, input, MAX_SOCKET_INPUT_SIZE, MSG_WAITALL | MSG_TRUNC, 
             (struct sockaddr *)&client, &clientAddrLen);
    // Check if input is valid
    if (strncmp(input, "ftp", 3) == 0) { // Input is valid
        printf("ftp message received\n");
        char yes[] = "yes";
        // Reply with "yes"
        sendto(sockfd, yes, strlen(yes), MSG_CONFIRM, (struct sockaddr *)&client, clientAddrLen);
    }
    else { // Input is not valid
        char no[] = "no";
        // Reply with "no"
        sendto(sockfd, no, strlen(no), MSG_CONFIRM, (struct sockaddr *)&client, clientAddrLen);
    }
    close(sockfd);
    return 0;
}