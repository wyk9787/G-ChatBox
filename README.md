# G-ChatBox

A P2P chat box implemented in C which provides both scalability and fault tolerance.

## Features

* Scalability

The system scales to many thousands of users without the need for a massive 
server or high-end internet connection. That means clients are not connecting 
to a single central server. Each peer(client) is itself both a client and a
server.

* Reliability

The system tolerates failure of any individual peer node in the service
including the failure of very first node joined the network; there 
is no single point of failure. In the case of failure of directory server,
already joined peers will be able to proceed their conversation as usual.

* Network Partition Avoidance

The way directory server helps each client join the network avoids the
possibility of network partition entirely.


## Usage

### Server side

```
./server
```

### Client side

```
./client <username> <server address>
```
