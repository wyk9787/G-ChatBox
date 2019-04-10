#include "share.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Initialize the lock
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#include "ui.h"

// Set up this client as a server to listen to incoming connection
void setup_server();

// Make a thread to run as a server to listen to connection
void *server_thread_fn(void *p);

// Make a thread to run as a client to read from parents and children
void *client_thread_fn(void *p);

// Sending request to directory server: CLIENT_JOIN, CLIENT_EXIT or REQUEST_PEER
void contact_directory_server(int type);

// Relay message to every children and parent except sender_socket
void relay(char *message, int sender_socket, char *username);

// Remove a child from the lined list when the child exists
void remove_child(int socket_fd);

// Adds lock around ui_add_message to safely display messages on UI
void display_meesage(char *username, char *message);

typedef struct node {
  int socket;
  struct node *next;
} node_t;

int parent_socket;
node_t *children_socket;

// Id of this client
int CLIENT_ID;

// Id of parent
int PARENT_ID;

// Name of this client
char *LOCAL_USER;

// Directory server's address
char *DIR_SERVER_ADDRESS;

// The port this client is listening on as a server
int SERVER_PORT;

// The socket this client uses as a server
int SERVER_SOCKET;

void display_meesage(char *username, char *message) {
  pthread_mutex_lock(&mutex);
  ui_add_message(username, message);
  pthread_mutex_unlock(&mutex);
}

void DEBUG(char *username, char *message) {
  // pthread_mutex_lock(&mutex);
  // ui_add_message(username, message);
  // pthread_mutex_unlock(&mutex);
}

void setup_server() {
  // Set up a socket
  int s = socket(AF_INET, SOCK_STREAM, 0);
  if (s == -1) {
    perror("socket failed");
    exit(2);
  }

  // Listen at this address. We'll bind to port 0 to accept any available port
  struct sockaddr_in addr = {.sin_addr.s_addr = INADDR_ANY,
                             .sin_family = AF_INET,
                             .sin_port = htons(0)};

  // Bind to the specified address
  if (bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
    perror("bind");
    exit(2);
  }

  // Become a server socket
  listen(s, 2);

  // Get the listening socket info so we can find out which port we're using
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getsockname(s, (struct sockaddr *)&addr, &addr_size);

  // Print the port information
  printf("Listening on port %d\n", ntohs(addr.sin_port));

  // Store port and socket
  SERVER_PORT = addr.sin_port;
  SERVER_SOCKET = s;
}

void *server_thread_fn(void *p) {

  int client_count = 0; // Children client count

  // Repeatedly accept connections
  while (true) {
    // Accept a client connection
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);
    int client_socket = accept(SERVER_SOCKET, (struct sockaddr *)&client_addr,
                               &client_addr_len);

    // Add new socket to the global linkedlist
    node_t *new_socket = malloc(sizeof(node_t));
    if (!new_socket) {
      perror("malloc");
      exit(1);
    }
    new_socket->socket = client_socket;
    new_socket->next = children_socket;
    children_socket = new_socket;

    // Convert client ip into a string stored in ipstr
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ipstr, INET_ADDRSTRLEN);

    display_meesage("SYSTEM", "One client just joined the network!");

    relay("Onc client just joined the network!", -1, "SYSTEM");

    // Create the thread
    pthread_t client_thread;

    if (pthread_create(&client_thread, NULL, client_thread_fn,
                       &client_socket)) {
      perror("pthread_create failed");
      exit(EXIT_FAILURE);
    }

    client_count++;
  }

  close(SERVER_SOCKET);
}

void *client_thread_fn(void *p) {
  // Unpack the thread arguments
  int socket_fd = *(int *)p;
  while (1) {
    client_message_t m;
    int ret = read(socket_fd, &m, sizeof(client_message_t));
    if (ret == -1) {
      perror("read");
      exit(2);
    }

    if (ret == 0) {
      // We will not use this socket to read or write any more.
      close(socket_fd);

      // Add system message to the display
      DEBUG("SYSTEM", "One client exited from the conversation");

      if (socket_fd == parent_socket) {
        DEBUG("DEBUG", "Lost connection to parent");
        // Lost connection to the parent
        contact_directory_server(REQUEST_PEER);
      } else {
        DEBUG("DEBUG", "Lost connection to children");
        // Lost connection to the children
        remove_child(socket_fd);
      }

      return NULL;
    }

    display_meesage(m.username, m.message);

    relay(m.message, socket_fd, m.username);
  }
}

void relay(char *message, int sender_socket, char *username) {
  client_message_t m;
  size_t message_len = strlen(message);
  size_t username_len = strlen(username);
  memcpy(m.message, message, message_len);
  memcpy(m.username, username, username_len);
  m.message[message_len] = '\0';
  m.username[username_len] = '\0';
  // Relay to a child if the message is not sent by that child
  node_t *cur = children_socket;
  while (cur != NULL) {
    if (sender_socket != cur->socket) {
      DEBUG("DEBUG", "send to child");
      if (write(cur->socket, &m, sizeof(client_message_t)) == -1) {
        perror("write");
        exit(1);
      }
    }
    cur = cur->next;
  }

  // If I am not the root, and my parent is not the sender, send it to parent
  if (parent_socket != -1 && sender_socket != parent_socket) {
    DEBUG("DEBUG", "send to parent");
    if (write(parent_socket, &m, sizeof(client_message_t)) == -1) {
      perror("write");
      exit(1);
    }
  }
}

void contact_directory_server(int type) {
  // Connects to the directory server and gets a list of peers
  struct hostent *dir_server = gethostbyname(DIR_SERVER_ADDRESS);
  if (dir_server == NULL) {
    fprintf(stderr, "Unable to find host %s\n", DIR_SERVER_ADDRESS);
    exit(1);
  }

  // Set up the address
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(DIR_SERVER_PORT)};

  bcopy((char *)dir_server->h_addr, (char *)&addr.sin_addr.s_addr,
        dir_server->h_length);

  struct sockaddr_in parent_addr = {.sin_family = AF_INET};

  // Keep track the number of tries happened
  int try_count = 0;

  // Make a sepcial case for requesting peer
  if (type == REQUEST_PEER) {
    try_count = 1;
  }

  int parent_s = -1;
  int res = -1;

  // Connect with the directory server
  do {
    // Close the previous iteration of parent_s
    if (parent_s != -1) {
      close(parent_s);
    }

    // Set up a socket for the server
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
      perror("socket failed");
      exit(2);
    }

    // Connect with the directory server
    if (connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
      perror("connect failed");
      exit(2);
    }

    message_t message;

    // Send the message to the directory server
    if (type == CLIENT_EXIT) {
      message.message_type = CLIENT_EXIT;
      message.id = CLIENT_ID;
    } else if (try_count == 0) { // Join the network
      message.message_type = CLIENT_JOIN;
      message.port = SERVER_PORT;
    } else { // Request a new peer
      message.message_type = REQUEST_PEER;
      message.id = CLIENT_ID;
      message.parent_id = PARENT_ID;
    }

    int bytes_write = write(s, &message, sizeof(message_t));
    if (bytes_write < 0) {
      perror("write failed");
      exit(2);
    }

    // If we are exiting, we don't need to wait for answer
    if (type == CLIENT_EXIT) {
      // Close the socket to the server
      close(s);
      return;
    }

    // Read the response message from the directory server
    response_t response;
    int bytes_read = read(s, &response, sizeof(response_t));
    if (bytes_read < 0) {
      perror("read failed");
      exit(2);
    }

    PARENT_ID = response.parent_id;

    char p[30];
    sprintf(p, "Parent id: %d", PARENT_ID);
    DEBUG("DEBUG", p);

    // Close the socket to the server
    close(s);

    if (response.is_root) { // This is the root of the network
      CLIENT_ID = response.id;
      parent_socket = -1;
      return;
    }

    // Set up a socket for parent
    parent_s = socket(AF_INET, SOCK_STREAM, 0);
    if (parent_s == -1) {
      perror("socket failed");
      exit(2);
    }

    // Set up the address
    parent_addr.sin_addr.s_addr = response.ip_addr;
    parent_addr.sin_port = response.port;

    // Only if we are trying to join the network, we need a new ID.
    if (message.message_type == CLIENT_JOIN) {
      CLIENT_ID = response.id;
    }

    // Keep track of how many times we have retried.
    try_count++;

    // Try to connect
    res = connect(parent_s, (struct sockaddr *)&parent_addr,
                  sizeof(struct sockaddr_in));
  } while (res != 0);

  parent_socket = parent_s;

  pthread_t client_thread;

  // Root client does not need to listen to its parent
  if (parent_socket != -1) {
    // Create thread to listen to parent
    if (pthread_create(&client_thread, NULL, client_thread_fn,
                       &parent_socket)) {
      perror("pthread_create failed");
      exit(EXIT_FAILURE);
    }
  }
}

void remove_child(int socket_fd) {
  node_t *cur = children_socket;
  node_t *prev = NULL;
  if (cur != NULL && cur->socket == socket_fd) {
    children_socket = cur->next;
    free(cur);
    return;
  }

  while (cur) {
    if (cur->socket == socket_fd) {
      prev->next = cur->next;
      free(cur);
      return;
    }
    prev = cur;
    cur = cur->next;
  }

  DEBUG("LIST:", "Didn't find!");
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <username> <server address>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  LOCAL_USER = argv[1];
  DIR_SERVER_ADDRESS = argv[2];

  // Initialize the linkedlist
  children_socket = NULL;

  parent_socket = -1;

  setup_server();
  contact_directory_server(CLIENT_JOIN);

  pthread_t server_thread;

  if (pthread_create(&server_thread, NULL, server_thread_fn, NULL)) {
    perror("pthread_create failed");
    exit(EXIT_FAILURE);
  }

  // Initialize the chat client's user interface.
  ui_init();
  // Add a test message
  display_meesage(NULL, "Type your message and hit <ENTER> to post.");

  // Loop forever
  while (true) {
    // Read a message from the UI
    char *message = ui_read_input();

    // If the message is a quit command, shut down. Otherwise print the message
    if (strcmp(message, "\\quit") == 0) {
      contact_directory_server(CLIENT_EXIT);
      break;
    } else if (strlen(message) > 0) {
      // Add the message to the UI
      display_meesage(LOCAL_USER, message);
      relay(message, -1, LOCAL_USER);
    }

    // Free the message
    free(message);
  }

  // Clean up the UI
  ui_shutdown();

  return 0;
}
