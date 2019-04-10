# G-ChatBox

G-ChatBox is a p2p chat box purely implemented in C that uses a single directory server to enter the
conversation. Any type of the crash on any node(any client or directory server) will __*NOT*__ 
affect the rest of the conversation.

## Usage

### Server side

```
./server
```

### Client side

```
./client <username> <server address>
```
