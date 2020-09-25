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

int main(int argc, char **argv) {
	if (argc < 3 || (atoi(argv[2]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using ./deliver <Server Address> <Server Port Number>\n");
        return 0;
    }

}