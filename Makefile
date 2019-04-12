CC = clang
CFLAGS = -g

all: peer directory_server

clean:
	rm -f directory_server
	rm -f peer

peer: peer.c ui.c ui.h share.h
	$(CC) $(CFLAGS) -o peer peer.c ui.c -lncurses -pthread

directory_server: directory_server.c share.h
	$(CC) $(CFLAGS) -o directory_server directory_server.c 
