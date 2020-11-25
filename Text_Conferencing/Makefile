CC := gcc
CFLAGS := -g -Wall
DEPS = packet.h server.h client.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server: server.o 
	$(CC) -o server server.o -lm -lpthread

client: client.o 
	$(CC) -o client client.o -lm -lpthread