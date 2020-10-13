#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>  
#include <assert.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <sys/wait.h>
#include <signal.h>

#include "packet.h"

#define MAX_SOCKET_INPUT_SIZE 128 // Bytes
#define MAX_PACKET_INPUT_SIZE 1200 // Bytes

void receiveFile(int sockfd, struct sockaddr_in *client, socklen_t *clientAddrLen);
Packet stringToPacket(char *input);

// Parts of this code was adapted from Beej's Guide to Network Programming
// Found online at: https://beej.us/guide/bgnet/html/

int main(int argc, char **argv) {
	if (argc < 2 || (atoi(argv[1]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using \"server <UDP Listen Port>\"\n");
        return 0;
    }
    char *portNum = argv[1];
    struct addrinfo hints, *serverInfo, *ptr;
    struct sockaddr_in client;  
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
    // struct ifaddrs *ifaddr, *ifPtr;
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
        //printf("ftp message received\n");
        char yes[] = "yes";
        // Reply with "yes"
        sendto(sockfd, yes, strlen(yes), MSG_CONFIRM, (struct sockaddr *)&client, clientAddrLen);
        // Receive file transfer
        receiveFile(sockfd, &client, &clientAddrLen);
    }
    else { // Input is not valid
        char no[] = "no";
        // Reply with "no"
        sendto(sockfd, no, strlen(no), MSG_CONFIRM, (struct sockaddr *)&client, clientAddrLen);
    }
    close(sockfd);
    return 0;
}

// Receive file over socket and store it
void receiveFile(int sockfd, struct sockaddr_in *client, socklen_t *clientAddrLen) {
    char *input = malloc(sizeof(char) * MAX_PACKET_INPUT_SIZE);
    recvfrom(sockfd, input, MAX_PACKET_INPUT_SIZE, MSG_TRUNC, 
             (struct sockaddr *)client, clientAddrLen);
    fprintf(stderr, "Packet received over socket\n");
    Packet packet = stringToPacket(input);
    assert(packet.fragNum == 1);
    int fd = creat(packet.filename, S_IRWXU);
    assert(fd >= 0);
    // Write contents to file
    int writeReturn = write(fd, packet.filedata, packet.size);
    if (writeReturn < 0)
        fprintf(stderr, "Error writing packet %u to new file\n", packet.fragNum);
    // Acknowledge the packet
    char *ack = (char *)malloc(sizeof(char) * 1028);
    strcpy(ack, "ACK2");
    int sendRet = sendto(sockfd, ack, strlen(ack), MSG_CONFIRM, (struct sockaddr *)client, *clientAddrLen);
    if (sendRet < 0)
        perror("Error");
    fprintf(stderr, "Sent %s\n", ack);
    // If there was only one packet we're done
    if (packet.totalFragments == 1)
        return;
    // Change socket to one second timeout so we can send NACK if we don't receive a packet in time
    struct timeval t;
    t.tv_sec = 1;
    t.tv_usec = 0;
    int sockOptRet = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
    if (sockOptRet < 0) 
        perror("Error");
    // Else get all packets, acknowledge them, and write their contents to the new file
    for (unsigned lastPackNum = 1; lastPackNum < packet.totalFragments; ++lastPackNum) {
        bool packetRecv = false;
        while (!packetRecv) {
            int recvVal = recvfrom(sockfd, input, MAX_PACKET_INPUT_SIZE, MSG_TRUNC, 
                                  (struct sockaddr *)client, clientAddrLen);
            if (recvVal > 0)
                packetRecv = true;
            else { // Didn't receive so send NACK
                fprintf(stderr, "Packet not received over socket, sending NACK\n");
                sprintf(ack, "NACK%u", lastPackNum + 1);
                int sendRet = sendto(sockfd, ack, strlen(ack), MSG_CONFIRM, (struct sockaddr *)client, *clientAddrLen);
                if (sendRet < 0)
                    perror("Error");
            }
        }
        fprintf(stderr, "Packet received over socket\n");
        Packet packet = stringToPacket(input);
        assert(packet.fragNum == (lastPackNum + 1));
        // Write contents to file
        writeReturn = write(fd, packet.filedata, packet.size);
        if (writeReturn < 0)
            fprintf(stderr, "Error writing packet %u to new file\n", lastPackNum);
        // Acknowledge the packet
        sprintf(ack, "ACK%u", packet.fragNum + 1);
        int sendRet = sendto(sockfd, ack, strlen(ack), MSG_CONFIRM, (struct sockaddr *)client, *clientAddrLen);
        if (sendRet < 0)
            perror("Error");
        fprintf(stderr, "Sent %s\n", ack);
    }
}

// Converts string we received over socket into packet
Packet stringToPacket(char *input) {
    Packet packet;
    char *totalFragStr = strtok(input, ":");
    char *fragNumStr = strtok(NULL, ":");
    char *sizeStr = strtok(NULL, ":");
    char *fileName = strtok(NULL, ":");
    packet.totalFragments = atoi(totalFragStr);
    packet.fragNum = atoi(fragNumStr);
    packet.size = atoi(sizeStr);
    packet.filename = fileName;
    if (packet.totalFragments <= 0 || packet.fragNum <= 0 || packet.size <= 0)
        fprintf(stderr, "Error in transmission\n");
    // Find index in string where data section starts. + 4 to account for ':'
    unsigned dataIndex = 4 + strlen(totalFragStr) + strlen(fragNumStr) + strlen(sizeStr) + strlen(fileName);
    memcpy(packet.filedata, input + dataIndex, packet.size);
    return packet;
}