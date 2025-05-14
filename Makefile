# TODO: make sure the rules for server client and markdown filled!

CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -Ilibs

all: markdown.o 


markdown.o: source/markdown.c libs/markdown.h
	$(CC) $(CFLAGS) -c source/markdown.c -o markdown.o

clean:
	rm -f markdown.o