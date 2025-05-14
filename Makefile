# TODO: make sure the rules for server client and markdown filled!
CC := gcc
CFLAGS := -Wall -Wextra

all: server client

server: server.o source/markdown.o
    $(CC) $(CFLAGS) -o server server.o source/markdown.o

client: client.o
    $(CC) $(CFLAGS) -o client client.o

server.o: server.c libs/markdown.h
    $(CC) $(CFLAGS) -c server.c -o server.o

client.o: client.c
    $(CC) $(CFLAGS) -c client.c -o client.o

source/markdown.o: source/markdown.c libs/markdown.h
    $(CC) $(CFLAGS) -c source/markdown.c -o source/markdown.o


clean:
    rm -f server client server.o client.o source/markdown.o markdown.o

