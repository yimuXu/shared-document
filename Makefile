# TODO: make sure the rules for server client and markdown filled!

CC := gcc
CFLAGS := -fsanitize=address -g -Wall -Wextra  -Ilibs -Ipthread
LDFLAGS := -fsanitize=address

all: server client 

server: server.o markdown.o command.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o server server.o markdown.o command.o

server.o: source/server.c libs/markdown.h
	$(CC) $(CFLAGS) -c source/server.c -o server.o

client: client.o markdown.o command.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o client client.o markdown.o command.o

client.o: source/client.c libs/markdown.h
	$(CC) $(CFLAGS) -c source/client.c -o client.o


markdown.o: source/markdown.c libs/markdown.h
	$(CC) $(CFLAGS) -c source/markdown.c -o markdown.o

command.o: source/command.c libs/command.h libs/markdown.h
	$(CC) $(CFLAGS) -c source/command.c -o command.o

clean:
	rm -f *.o server client