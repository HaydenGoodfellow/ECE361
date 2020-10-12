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
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_USER_INPUT_SIZE 128 // Chars
#define MAX_SOCKET_INPUT_SIZE 128 // Bytes
#define MAX_DATA_SIZE 1000 //Bytes

struct P{
    unsigned int total_frag;
    unsigned int frag_n;
    unsigned int size;
    char* fileName;
    char fileData[MAX_DATA_SIZE];
};

bool fileExists(char *name);
void sendPackets(char* name, int sockfd, struct addrinfo* serverInfo);
char* formatToString(struct P* packet);

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
    // removes the newline character
    userInput[strlen(userInput)-1] = '\0';
    char* fileName = userInput + 4;
    if (!fileExists(fileName)) { 
        fprintf(stderr, "File not found\n");
        return 0;
    }
    
    // Open socket 
    int sockfd = socket(AF_INET /*IPv4*/, SOCK_DGRAM /*UDP*/, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        return 0;
    }
    // Sets the server information
    char *serverHostName = argv[1]; 
    char *serverPortNumber = argv[2];
    struct addrinfo hints, *serverInfo;
    // Set up hints struct
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Ensures IPv4
    hints.ai_socktype = SOCK_DGRAM; // Ensures UDP
    // Get server info
    int getAddrReturn = getaddrinfo(serverHostName, serverPortNumber, &hints, &serverInfo);
    if (getAddrReturn != 0) {
        fprintf(stderr, "Failed to get address info: %s\n", gai_strerror(getAddrReturn));
        return 0;
    }
    
    // Send info to server
    struct sockaddr_in server;
    char ftp[] = "ftp";
    sendto(sockfd, ftp, strlen(ftp), MSG_CONFIRM, serverInfo->ai_addr, serverInfo->ai_addrlen);
    // Receive info back from server
    char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
    socklen_t serverAddrLen = sizeof(struct sockaddr);
    recvfrom(sockfd, input, MAX_SOCKET_INPUT_SIZE, MSG_WAITALL | MSG_TRUNC,
             (struct sockaddr *)&server, &serverAddrLen);
    if (strncmp(input, "yes", 3) == 0){
        printf("A file transfer can start\n");
        sendPackets(fileName, sockfd, serverInfo);
    }
    else{
        fprintf(stderr, "Response from server not \"yes\"");
    }
    return 0;
}

// Currently assuming the file is in our current working directory
bool fileExists(char *name) {
    //fprintf(stderr, "Name: %s\n", name);
    DIR *dir = opendir(".");
    struct dirent *entry = readdir(dir);
    while (entry != NULL) {
        if (strncmp(entry->d_name, name, strlen(entry->d_name)) == 0)
            return true;
        entry = readdir(dir);
    }
    return false;
}

// send packets to server
// waits for an acknowledgment from the server before sending the next packet
void sendPackets(char* name, int sockfd, struct addrinfo* serverInfo){
    FILE* file = fopen(name, "r"); 
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    int totalFrag = (size-1 + MAX_DATA_SIZE)/MAX_DATA_SIZE;
    printf("here0\n");
    printf("%d\n", totalFrag);
    
    printf("here1\n");
    for (int i = 1; i<=totalFrag; i++){
        struct P* packet = malloc(sizeof(struct P));
        packet->total_frag = totalFrag;
        packet->frag_n = i;
        packet->fileName = name;
        //!!!!!segmentation fault packet->fileData
        if (i == totalFrag){
            fseek(file, 0, SEEK_END);
            packet->size = ftell(file);
            fread((void*)packet->fileData, sizeof(char), packet->size, file);
        }
        else{
            packet->size = fread((void*)packet->fileData, sizeof(char), MAX_DATA_SIZE, file);
        }
        char* msg = formatToString(packet);
        sendto(sockfd, msg, strlen(msg), MSG_CONFIRM, serverInfo->ai_addr, serverInfo->ai_addrlen);
        
        // gets ack from server (listen)
        while(true){
            char *input = malloc(sizeof(char) * MAX_SOCKET_INPUT_SIZE);
            struct sockaddr_in server;
            socklen_t serverAddrLen = sizeof(struct sockaddr);
            recvfrom(sockfd, input, MAX_SOCKET_INPUT_SIZE, MSG_WAITALL | MSG_TRUNC,
             (struct sockaddr *)&server, &serverAddrLen);
            if (strcmp(input, "ack") == 0)
                break;
        }
    }
}

// formats the packet content to string
char* formatToString(struct P* packet){
    printf("here2\n");
    char result[5000];
    int offset = sprintf( result, "%d:%d:%d:%s:", packet->total_frag, packet->frag_n, packet->size, packet->fileName );
    memcpy( result + offset, packet->fileData, packet->size );
    return result;
}