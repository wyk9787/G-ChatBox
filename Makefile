CC = clang
CFLAGS = -g

all: client server

clean:
	rm -f client

client: client.c ui.c ui.h share.h
	$(CC) $(CFLAGS) -o client client.c ui.c -lncurses -pthread

server: server.c share.h
	$(CC) $(CFLAGS) -o server server.c 
