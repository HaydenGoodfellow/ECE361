CC := gcc
CFLAGS := -g -Wall -Werror

server: server.o 
	$(CC) -o server server.o

deliver: deliver.o 
	$(CC) -o deliver deliver.o