CC := gcc
CFLAGS := -g -Wall
DEPS = packet.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: server.o 
	$(CC) -o server server.o -lm

deliver: deliver.o 
	$(CC) -o deliver deliver.o -lm