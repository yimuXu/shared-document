# TODO: make sure the rules for server client and markdown filled!
CC := gcc
CFLAGS := -Wall -Wextra

all: server client

server: source/server.c source/markdown.o
	$(CC) $(CFLAGS) -o server server.o source/markdown.o

client: source/client.c 
	$(CC) $(CFLAGS) -o client source/client.c
# server.o: server.c
# 	$(CC) $(CFLAGS) -c server.c
# client.o: client.c
# 	$(CC) $(CFLAGS) -c client.c

markdown.o: source/markdown.c
	$(CC) $(CFLAGS) -c source/markdown.c -o source/markdown.o

clean:
	re -f server client source/*.o
