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

#define MAX_INPUT_SIZE 128

bool fileExists(char *name);

int main(int argc, char **argv) {
    // Ensure valid arguments are passed in
	if (argc < 3 || (atoi(argv[2]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using \"deliver <Server Address> <Server Port Number>\"\n");
        return 0;
    }
    // Recieve the file name from the user and ensure its valid
    char input[MAX_INPUT_SIZE];
    bool validInput = false;
    while (!validInput) {
        printf("Please input a file name using the following format:\nftp <File Name>\n");
        fgets(input, MAX_INPUT_SIZE, stdin);
        // Ensures user entered "ftp " at the start of input
        if (strncmp("ftp ", input, 4) != 0) {
            fprintf(stderr, "Invalid input, please try again\n");
            continue;
        }
        validInput = true;
    }
    if (!fileExists(input + 4)) { // Plus 4 to remove the "ftp "
        fprintf(stderr, "File not found\n");
        return 0;
    }
    // Open socket 

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