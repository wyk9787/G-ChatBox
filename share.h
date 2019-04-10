#ifndef SHARE_HPP
#define SHARE_HPP

#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <assert.h>

#define DIR_SERVER_PORT 4444

#define CLIENT_JOIN 1
#define REQUEST_PEER 2
#define CLIENT_EXIT 3

typedef struct _thread_arg {
  int socket_fd;
  int client_number;
  char *username;
  uint32_t client_ip;
} thread_arg_t;

typedef struct _server_thread_arg {
  int socket_fd;
  char *username;
} server_thread_arg_t;

typedef struct _message {
  int message_type;
  in_port_t port;
  int id;
  int parent_id; // Tried parent id that doesn't connect
} message_t;

typedef struct _client_message {
  char message[80];
  char username[80];
} client_message_t;

typedef struct _response {
  in_port_t port; // Parent's port
  uint32_t ip_addr; // Parent's ip
  int id; // Client's id
  bool is_root;
  int parent_id;
} response_t;

typedef struct _client{
  in_port_t port;
  uint32_t ip_addr;
  int id;
  bool active;
} client_t;

#endif
