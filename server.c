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

int main(int argc, char **argv) {
	if (argc < 2 || (atoi(argv[1]) <= 0)) {
        fprintf(stderr, "Incorrect usage. Please invoke using ./server <UDP Listen Port>\n");
        return 0;
    }

}