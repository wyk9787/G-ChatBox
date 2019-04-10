# G-ChatBox

A P2P chat box implemented in C which provides both scalability and fault tolerance.

* Scalability

The system should scale to many thousands of users without the need for a massive 
server or high-end internet connection. That means clients should not all connect to a single central server.

* Reliability

The system should tolerate failure of any individual node in the service; there 
should be no single point of failure.


## Usage

### Server side

```
./server
```

### Client side

```
./client <username> <server address>
```
