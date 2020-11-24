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
        login();
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

void login(){
    printf("login\n");
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

