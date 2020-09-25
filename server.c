#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>

#define MAX_SOCKET_INPUT_SIZE 128 // Bytes

int main(int argc, char **argv) {
	if (argc < 2 || (atoi(argv[1]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using \"server <UDP Listen Port>\"\n");
        return 0;
    }
    char *portNum = argv[1];
    struct addrinfo hints, *serverInfo, *ptr;
    struct sockaddr_in client;
    struct ifaddrs *ifaddr, *ifPtr;
    // Set up hints struct
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET; // Ensures IPv4
    hints.ai_socktype = SOCK_DGRAM; // Ensures UDP
    // Get address info
    int getAddrReturn = getaddrinfo(NULL, portNum, &hints, &serverInfo);
    if (getAddrReturn != 0) {
        fprintf(stderr, "Failed to get address info: %s\n", gai_strerror(getAddrReturn));
        return 0;
    }
    // Set up socket
    int sockfd = socket(AF_INET /*IPv4*/, SOCK_DGRAM /*UDP*/, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 0;
    }
    // Bind socket
    ptr = serverInfo;
    while (ptr != NULL) {
        int bindReturn = bind(sockfd, ptr->ai_addr, ptr->ai_addrlen);
        if (bindReturn < 0) {
            fprintf(stderr, "Failed to bind socket\n");
            ptr = ptr->ai_next;
            continue;
        }
        else
            break;
    }
    if (ptr == NULL) // Wasn't able to bind socket so exit program
        return 0;
    // Print the server computer's ip address
    // getifaddrs(&ifaddr);
    // for (ifPtr = ifaddr; ifPtr != NULL; ifPtr = ifPtr->ifa_next) {
    //     if (ifPtr->ifa_addr->sa_family == AF_INET) {
    //         char *ipaddr = inet_ntoa(((struct sockaddr_in *)ifPtr->ifa_addr)->sin_addr);
    //         if (strncmp(ipaddr, "127.", 4) != 0) // Ignore loopback address
    //         printf("Server computer's IP address: %s\n", ipaddr);
    //     }
    // }
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