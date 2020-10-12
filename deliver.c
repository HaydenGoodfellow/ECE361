#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>    
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#include "packet.h"

#define MAX_USER_INPUT_SIZE 128 // Chars
#define MAX_SOCKET_INPUT_SIZE 128 // Bytes

void transferFile(int sockfd, char *filename, struct addrinfo *serverInfo);
char *packetToString(Packet packet);
bool fileExists(char *name);
off_t getFileSize(int fd);

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
    if (strncmp(input, "yes", 3) == 0) {
        printf("A file transfer can start\n");
        transferFile(sockfd, userInput + 4, serverInfo);
    }
    else
        fprintf(stderr, "Response from server not \"yes\"");
    return 0;
}

// Transfer a file to server over socket sockfd
void transferFile(int sockfd, char *name, struct addrinfo *serverInfo) {
    // Get proper pathname to file by stripping whitespaces
    char *end = name + strlen(name) - 1;
    while ((end > name) && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';

    // char *srcPath = (char *)malloc(sizeof(char) * MAX_USER_INPUT_SIZE);
    // // - 1 to remove the '\n' character from being read
    // strncpy(srcPath, name, strlen(name) - 1);
    // srcPath[strlen(name) - 1] = '\0';
    // Open file
    int srcFile = open(name, O_RDONLY);
    if (srcFile < 0)
        fprintf(stderr, "Error opening file: %s\n", name);
    // Determine number of fragments needed
    long fileSize = getFileSize(srcFile);
    unsigned totalNumFragments = (unsigned) ceil(((double) fileSize) / 1000.0);
    fprintf(stderr, "Total number of fragments: %u\n", totalNumFragments);
    // Send fragments one by one
    for (int fragNum = 1; fragNum <= totalNumFragments; ++fragNum) {
        fprintf(stderr, "fragNum: %d\n", fragNum);
        Packet packet = {totalNumFragments, fragNum, 0, name};
        char buf[1000];
        int readReturn = read(srcFile, buf, 1000);
        if (readReturn < 0)
            fprintf(stderr, "Error reading from file: %s\n", name);
        packet.size = readReturn;
        memcpy(packet.filedata, buf, readReturn);
        
    }
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

// Converts packet to a string we can send over the socket
char *packetToString(Packet pk) {
    int bytesPrinted = 0;
    // Allocate on heap a string with enough size for everything but data
    char *result = (char *)malloc(sizeof(char) * 100);
    // Print everything but data into the string
    bytesPrinted = snprintf(result, 100, "%u:%u:%u:%s:", pk.totalFragments, pk.fragNum, pk.size, pk.filename);
    fprintf(stderr, "String without data: %s\nLength: %lu\n", result, strlen(result));
    // Resize string to have space to store data
    result = (char *)realloc(result, bytesPrinted + 1 + pk.size);
    // + 1 so it inserts data where snprintf put '\0'
    memcpy(result + bytesPrinted + 1, pk.filedata, pk.size);
    fprintf(stderr, "String with data: ");
    for (int i = 0; i < bytesPrinted + 1 + pk.size; ++i)
        fprintf(stderr, "%c", result[i]);
    fprintf(stderr, "\n");
    return result;
}

// Get size of file in bytes
off_t getFileSize(int fd) {
    struct stat fileStats;
    if (fstat(fd, &fileStats) == -1)
        fprintf(stderr, "Error reading file stats\n");
    return fileStats.st_size;
}