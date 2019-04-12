#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "share.h"

// Arraylist to keep track of all clients
client_t *client_list;

// Capacity of the arraylist
int capacity;

// Number of clients we have connected so far
int client_num;

// Keep track of the root client of this network
int root_client;

void send_parent(int socket, int id) {
  printf("Will send response to id %d\n", id);
  int index = -1;

  // Iterate the list to see if there is at least one client is active
  bool one_active = false;

  printf("Debug: ");
  for (int i = 0; i < id; i++) {
    if (client_list[i].active) {
      one_active = true;
      break;
    }
    printf("%d ", client_list[i].active);
  }
  printf("\n");

  // If there is at least one active potential parent
  if (one_active) {
    do {
      // We only want to send back the client that has a smaller id than the
      // current client
      index = rand() % id;
    } while (!client_list[index].active);
  }

  response_t response = {.port = client_list[index].port,
                         .ip_addr = client_list[index].ip_addr,
                         .id = id,
                         .is_root = false,
                         .parent_id = index};

  if (!one_active) {
    response.is_root = true;
  }

  printf("Sending: id = %d, port = %d, is_root = %d, parent_id = %d\n",
         response.id, ntohs(response.port), response.is_root,
         response.parent_id);
  if (write(socket, &response, sizeof(response_t)) == -1) {
    perror("write");
    exit(1);
  }
}

void handle_client(int socket_fd, uint32_t client_ip) {
  message_t message;
  if (read(socket_fd, &message, sizeof(message_t)) == -1) {
    perror("read");
    exit(1);
  }

  if (message.message_type == CLIENT_JOIN) {  // Request to join
    printf("Receive CLIENT_JOIN request\n");
    // Ensure capacity
    if (client_num > capacity) {
      capacity *= 2;
      client_list = realloc(client_list, capacity * sizeof(client_t));
      if (!client_list) {
        perror("realloc");
        exit(1);
      }
    }

    // Add new client to our arraylist
    client_list[client_num].port = message.port;
    client_list[client_num].id = client_num;
    client_list[client_num].ip_addr = client_ip;
    client_list[client_num].active = true;
    // Send message back
    send_parent(socket_fd, client_num);
    client_num++;
  } else if (message.message_type == REQUEST_PEER) {  // Reuquest new peer
    printf("Receive REQUEST_PEER request from %d\n", message.id);
    printf("Client %d is disactivated\n", message.parent_id);
    client_list[message.parent_id].active = false;
    // Send message back
    send_parent(socket_fd, message.id);
  } else if (message.message_type == CLIENT_EXIT) {  // Report to exit
    printf("Receive CLIENT_EXIT request from %d\n", message.id);
    printf("Client %d is disactivated\n", message.id);
    client_list[message.id].active = false;
    printf("Client %d exited.\n", message.id);
  }
}

int main() {
  // Seed the random generator
  srand(time(NULL));

  // Initialize our arraylist
  client_num = 0;
  capacity = 10;
  client_list = malloc(10 * sizeof(client_t));
  if (!client_list) {
    perror("malloc");
    exit(1);
  }

  int s = socket(AF_INET, SOCK_STREAM, 0);

  if (s == -1) {
    perror("socket failed");
    exit(2);
  }

  struct sockaddr_in addr = {.sin_addr.s_addr = INADDR_ANY,
                             .sin_family = AF_INET,
                             .sin_port = htons(DIR_SERVER_PORT)};

  if (bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
    perror("bind failed");
    exit(2);
  }

  if (listen(s, 2)) {
    perror("listen failed");
    exit(2);
  }

  // Get the listening socket info so we can find out which port we're using
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getsockname(s, (struct sockaddr *)&addr, &addr_size);

  // Print the port information
  printf("Listening on port %d\n", ntohs(addr.sin_port));

  // Count the number of conncetion this server has received
  int connection_count = 0;

  // Repeatedly accept connections
  while (true) {
    // Accept a client connection
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);
    int client_socket =
        accept(s, (struct sockaddr *)&client_addr, &client_addr_len);
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ipstr, INET_ADDRSTRLEN);

    uint32_t client_ip = client_addr.sin_addr.s_addr;

    printf("%dth connection connected.\n", connection_count);

    handle_client(client_socket, client_ip);
    close(client_socket);

    printf("%dth connection disconnected.\n\n", connection_count);

    connection_count++;
  }

  close(s);
}
