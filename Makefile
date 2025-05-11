# TODO: make sure the rules for server client and markdown filled!
CC := gcc
CFLAGS := -Wall -Wextra

all: server client

server: server.c markdown.o
	$(CC) $(CFLAGS) -o server server.o markdown.o

client: client.c 
	$(CC) $(CFLAGS) -o client client.c
# server.o: server.c
# 	$(CC) $(CFLAGS) -c server.c
# client.o: client.c
# 	$(CC) $(CFLAGS) -c client.c

markdown.o: markdown.c
	$(CC) $(CFLAGS) -c markdown.c -o markdown.o

clean:
	re -f server client source/*.o
