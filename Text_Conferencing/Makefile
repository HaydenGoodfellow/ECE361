CC = gcc
CFLAGS = -g -Wall
LINKFLAGS = -lm -lpthread
DEPS = message.h server.h client.h 
SERVEROBJS = list_functions.o server.o 
	
server: $(SERVEROBJS)
	$(CC) $(LINKFLAGS) -o server server.c list_functions.c

client: client.o 
	$(CC) $(LINKFLAGS) -o client client.c 

server.o: list_functions.c server.c server.h message.h 
list_functions.o: server.h
client.o: client.h message.h

# remove object files and other junk
clean:
	rm -f client.o server.o list_functions.o core