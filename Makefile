CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGETS = server client

all: $(TARGETS)

server: src/server.c src/protocol.h
	$(CC) $(CFLAGS) -o server src/server.c

client: src/client.c src/protocol.h
	$(CC) $(CFLAGS) -o client src/client.c

clean:
	rm -f $(TARGETS)

.PHONY: all clean

