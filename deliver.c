#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>    
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_USER_INPUT_SIZE 128 // Chars
#define MAX_SOCKET_INPUT_SIZE 128 // Bytes

bool fileExists(char *name);

// Parts of this code was adapted from Beej's Guide to Network Programming
// Found online at: https://beej.us/guide/bgnet/html/

int main(int argc, char **argv) {
    // Ensure valid arguments are passed in
	if (argc < 3 || (atoi(argv[2]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using \"deliver <Server Address> <Server Port Number>\"\n");
        return 0;
    }
    // Recieve the file name from the user and ensure its valid
    char userInput[MAX_USER_INPUT_SIZE];
    bool validInput = false;
    while (!validInput) {
        printf("Please input a file name using the following format:\nftp <File Name>\n");
        fgets(userInput, MAX_USER_INPUT_SIZE, stdin);
        // Ensures user entered "ftp " at the start of input
        if (strncmp("ftp ", userInput, 4) != 0) {
            fprintf(stderr, "Invalid input, please try again\n");
            continue;
        }
        validInput = true;
    }
    if (!fileExists(userInput + 4)) { // Plus 4 to remove the "ftp "
        fprintf(stderr, "File not found\n");
        return 0;
    }
    // Open socket 
    int sockfd = socket(AF_INET /*IPv4*/, SOCK_DGRAM /*UDP*/, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 0;
    }
    char *hostname = argv[1]; // Hostname or IPv4 address
    char *portNum = argv[2];
    struct addrinfo hints, *serverInfo;
    struct sockaddr_in server;
    // Set up hints struct
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Ensures IPv4
    hints.ai_socktype = SOCK_DGRAM; // Ensures UDP
    // Get address info
    int getAddrReturn = getaddrinfo(hostname, portNum, &hints, &serverInfo);
    if (getAddrReturn != 0) {
        fprintf(stderr, "Failed to get address info: %s\n", gai_strerror(getAddrReturn));
        return 0;
    }
    // Timer variables to measure RTT
    struct timeval send, recv;
    // Send info to server
    char ftp[] = "ftp";
    gettimeofday(&send, NULL);
    sendto(sockfd, ftp, strlen(ftp), MSG_CONFIRM, serverInfo->ai_addr, serverInfo->ai_addrlen);
    // Receive info back from server
    char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
    socklen_t serverAddrLen = sizeof(struct sockaddr);
    recvfrom(sockfd, input, MAX_SOCKET_INPUT_SIZE, MSG_WAITALL | MSG_TRUNC,
             (struct sockaddr *)&server, &serverAddrLen);
    gettimeofday(&recv, NULL);
    double rtt = (recv.tv_sec - send.tv_sec) * 1000 * 1000; // us
    rtt += ((recv.tv_usec - send.tv_usec)); // us
    printf("RTT is %.0f us\n", rtt);
    if (strncmp(input, "yes", 3) == 0) 
        printf("A file transfer can start\n");
    else
        fprintf(stderr, "Response from server not \"yes\"");
    return 0;
}

// Currently assuming the file is in our current working directory
bool fileExists(char *name) {
    //fprintf(stderr, "Name: %s\n", name);
    DIR *dir = opendir(".");
    struct dirent *entry = readdir(dir);
    while (entry != NULL) {
        //fprintf(stderr, "File name: %s\n", entry->d_name);
        // Using strncmp with length to avoid needing \0 at end of strings
        if (strncmp(entry->d_name, name, strlen(entry->d_name)) == 0)
            return true;
        entry = readdir(dir);
    }
    return false;
}