#include "broadcast.h"
#include "netutils.h"
#include "protocol.h"
#include <pthread.h>
#include <stdlib.h>

// Initialize global variables
Peer *peers = NULL;
int peer_count = 0;
pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;

// Our identity info
char my_token[TOKEN_LENGTH + 1];
char my_username[MAX_USERNAME_LENGTH + 1];

void init_my_info(void) {
  generate_token(my_token);

  struct passwd *pw = getpwuid(getuid());
  if (pw) {
    strcpy(my_username, pw->pw_name);
  } else {
    strcpy(my_username, "Unknown");
  }
}

void init_peer(Peer *peer, const char *ip, const char *token,
               const char *username) {
  strncpy(peer->ip, ip, INET_ADDRSTRLEN);
  peer->ip[INET_ADDRSTRLEN - 1] = '\0';

  strncpy(peer->token, token, TOKEN_LENGTH);
  peer->token[TOKEN_LENGTH] = '\0';

  strncpy(peer->username, username, MAX_USERNAME_LENGTH);
  peer->username[MAX_USERNAME_LENGTH] = '\0';

  peer->last_seen = time(NULL);
  peer->next = NULL;
}

void update_peer(const char *ip, const char *token, const char *username) {
  pthread_mutex_lock(&peers_mutex);

  // Check if peer already exists by token.
  // Update the last_seen, IP and username if so.
  Peer *peer = peers;
  int found = 0;

  while (peer != NULL && !found) {
    if (strcmp(peer->token, token) == 0) {
      peer->last_seen = time(NULL);

      // Check if the IP has changed
      if (strcmp(peer->ip, ip)) {
        strncpy(peer->ip, ip, INET_ADDRSTRLEN - 1);
        peer->ip[INET_ADDRSTRLEN - 1] = '\0';
      }

      // Check if the username has changed
      if (strcmp(peer->username, username)) {
        strncpy(peer->username, username, MAX_USERNAME_LENGTH);
        peer->username[MAX_USERNAME_LENGTH] = '\0';
      }

      found = 1;
    }

    peer = peer->next;
  }

  // If the peer is new, add it to the beginning of the list
  if (!found) {
    peer = malloc(sizeof(Peer));
    init_peer(peer, ip, token, username);

    Peer *prev_head = peers;
    peers = peer;
    peer->next = prev_head;

    peer_count++;
  }

  pthread_mutex_unlock(&peers_mutex);
}

void remove_stale_peers() {
  pthread_mutex_lock(&peers_mutex);

  time_t current_time = time(NULL);
  Peer *peer = peers;
  Peer *previous = NULL;

  while (peer != NULL) {
    if (current_time - peer->last_seen > STALE_PEER_TIMEOUT) {
      if (previous == NULL) {
        // Removing the head of the list
        peers = peer->next;
      } else {
        // Removing an item after the head of the list
        previous->next = peer->next;
      }

      Peer *temp = peer;
      peer = peer->next;
      free(temp);
    } else {
      previous = peer;
      peer = peer->next;
    }
  }

  pthread_mutex_unlock(&peers_mutex);
}

void *handle_broadcast(void *arg) {
  int sock = bind_broadcast_socket(BROADCAST_PORT);
  if (sock < 0) {
    exit(EXIT_FAILURE);
  }

  // Create the response socket
  int response_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (response_sock < 1) {
    perror("socket() failed");
    exit(EXIT_FAILURE);
  }

  uint8_t buffer[MAX_MESSAGE_LENGTH];
  struct PeerMessage msg;

  while (1) {
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    // Receive broadcast message
    ssize_t recv_len = recvfrom(sock, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&sender_addr, &addr_len);

    if (recv_len > 0) {
      // Skip if there's a deserialization error
      if (deserialize_message(buffer, &msg)) {
        continue;
      }

      char sender_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);

      // Don't respond to our own broadcasts
      if (strcmp(msg.token, my_token) == 0) {
        continue;
      }

      // Ignore response messages
      if (msg.header.is_response) {
        continue;
      }

      // Send response to broadcast messages
      uint8_t response_buffer[MAX_MESSAGE_LENGTH];
      int res_length =
          serialized_response(my_token, my_username, response_buffer);
      if (res_length < 0) {
        fprintf(stderr, "There was a problem serializing the response.\n");
        exit(EXIT_FAILURE);
      }

      sender_addr.sin_port = htons(RESPONSE_PORT);
      sendto(response_sock, response_buffer, res_length, 0,
             (struct sockaddr *)&sender_addr, sizeof(sender_addr));
    }
  }
}

void *send_broadcast(void *arg) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket() failed");
    exit(EXIT_FAILURE);
  }

  // Enable broadcast
  int broadcast = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast,
                 sizeof(broadcast)) < 0) {
    perror("setsockopt() failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(BROADCAST_PORT),
                             .sin_addr.s_addr = inet_addr(BROADCAST_IP)};

  uint8_t buffer[MAX_MESSAGE_LENGTH];
  int msg_length = serialized_broadcast(my_token, my_username, buffer);

  while (1) {
    sendto(sock, buffer, msg_length, 0, (struct sockaddr *)&addr, sizeof(addr));

    // TODO: cleanup stale peers

    sleep(DISCOVERY_INTERVAL);
  }
}

void *handle_responses(void *arg) {
  // This socket will be bound to the RESPONSE_PORT, and will be used
  // to listen to responses from peers who respond to this machine's
  // broadcast message.
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket() failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(RESPONSE_PORT),
                             .sin_addr.s_addr = INADDR_ANY};

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind() failed");
    exit(EXIT_FAILURE);
  }

  uint8_t buffer[MAX_MESSAGE_LENGTH];
  struct PeerMessage msg;

  while (1) {
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    ssize_t bytes_received =
        recvfrom(sock, buffer, sizeof(buffer), 0,
                 (struct sockaddr *)&sender_addr, &addr_len);

    if (bytes_received > 0) {
      if (deserialize_message(buffer, &msg)) {
        continue;
      }

      char sender_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);
      update_peer(sender_ip, msg.token, msg.username);
    }
  }
}
