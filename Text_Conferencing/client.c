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

#include "client.h"

#define MAX_USER_INPUT_SIZE 128 // Chars
#define MAX_USER_NAME 128
#define MAX_USER_PWD 128

// commands
const char* login_cmd = "/login";
const char* logout_cmd = "/logout";
const char* join_session_cmd = "/joinsession";
const char* leave_session_cmd = "/leavesession";
const char* create_session_cmd = "/createsession";
const char* list_cmd = "/list";
const char* quit_cmd = "/quit";

int main(int argc, char* argv[]){
    fd_set fd;
    while(1){
        get_cmd();
    }
    return 0;
}

int get_cmd(){
    char input[MAX_USER_INPUT_SIZE];
    int err = 0;
    
    scanf("%s", input);
    if (strcmp(input, login_cmd) == 0){
        char user_name[MAX_USER_NAME];
        char user_pwd[MAX_USER_PWD];
        char server_ip[MAX_USER_INPUT_SIZE];
        char server_port[MAX_USER_INPUT_SIZE];
        scanf(" %s %s %s %s", user_name, user_pwd, server_ip, server_port);
        login(user_name, user_pwd, server_ip, server_port);
    }
    else if(strcmp(input, logout_cmd) == 0){
        logout();
    }
    else if(strcmp(input, join_session_cmd) == 0){
        join_session();
    }
    else if((strcmp(input, leave_session_cmd) == 0)){
        leave_session();
    }
    else if((strcmp(input, create_session_cmd) == 0)){
        create_session();
    }
    else if((strcmp(input, list_cmd) == 0)){
        list();
    }
    else if((strcmp(input, quit_cmd) == 0)){
        quit();
    }
    return err;
}

void login(const char* name, const char* pass, const char* server_ip, const char* server_port){
    printf("login\n");
    // create socket
    struct addrinfo hints, *serverInfo;
    int sockfd;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Ensures IPv4
    hints.ai_socktype = SOCK_STREAM; // Ensures TCP
    printf("name: %s\n", name);
    printf("pass: %s\n", pass);
    printf("server ip: %s\n", server_ip);
    printf("server port: %s\n", server_port);
    // Get address info
    int getAddrReturn = getaddrinfo(server_ip, server_port, &hints, &serverInfo);
    if (getAddrReturn != 0) {
        fprintf(stderr, "Failed to get address info: %s\n", gai_strerror(getAddrReturn));
        return;
    }
    struct addrinfo* i;
    for ( i = serverInfo; i!= NULL; i = i->ai_next){
        sockfd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if (sockfd == -1){
            continue;
        }
        if (connect(sockfd, i->ai_addr, i->ai_addrlen) == -1){
            close(sockfd);
            continue;
        }
        break;
    }
    // check if client is connected
    if (i == NULL){
        printf("client: fail to login");
        return;
    }
    // create and send packet to server, indicating login
    
    return;
}
void logout(){
    printf("logout\n");
    return;
}
void join_session(){
    printf("join session\n");
    return;
}
void leave_session(){
    printf("leave session\n");
    return;
}
void create_session(){
    printf("create session\n");
    return;
}
void list(){
    printf("list\n");
    return;
}
void quit(){
    printf("quit\n");
    return;
}

