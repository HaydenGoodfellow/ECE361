#include <stdio.h>
#include <stdlib.h>

typedef struct Packet {
    unsigned int totalFragments;
    unsigned int fragNum;
    unsigned int size;
    char* filename;
    char filedata[1000];
} Packet;

