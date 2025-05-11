# TODO: make sure the rules for server client and markdown filled!
# CC := gcc
# CFLAGS := -Wall -Wextra -std=c11 -Ilibs

# all: server client markdown.o

# server: server.c markdown.o
# 	$(CC) $(CFLAGS) -o server server.o markdown.o

# client: client.c 
# 	$(CC) $(CFLAGS) -o client client.c
# # server.o: server.c
# # 	$(CC) $(CFLAGS) -c server.c
# # client.o: client.c
# # 	$(CC) $(CFLAGS) -c client.c

# markdown.o: source/markdown.c libs/markdown.h
# 	$(CC) $(CFLAGS) -c source/markdown.c -o source/markdown.o

# clean:
# 	rm -f server client source/*.o
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -Ilibs

all: markdown.o

# 编译 markdown.o（供学校测试使用）
markdown.o: source/markdown.c libs/markdown.h
	$(CC) $(CFLAGS) -c source/markdown.c -o markdown.o

clean:
	rm -f markdown.o