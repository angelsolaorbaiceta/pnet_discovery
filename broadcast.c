#include "broadcast.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

// Initialize global variables
Peer peers[MAX_PEERS];
int peer_count = 0;
pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;

static int is_own_ip(char *sender_ip) {
  char buffer[100];
  gethostname(buffer, sizeof(buffer));

  char host_ip[INET_ADDRSTRLEN];
  struct hostent *host_entry = gethostbyname(buffer);
  strcpy(host_ip, inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0])));

  return strcmp(sender_ip, host_ip);
}

void update_peer(const char *ip) {
  pthread_mutex_lock(&peers_mutex);

  // Check if peer already exists.
  // Update the last_seen if so.
  int found = 0;
  for (int i = 0; i < peer_count; i++) {
    if (strcmp(peers[i].ip, ip) == 0) {
      peers[i].last_seen = time(NULL);
      found = 1;
      break;
    }
  }

  // If the peer is new, add it if there is space
  if (!found && peer_count < MAX_PEERS) {
    strcpy(peers[peer_count].ip, ip);
    peers[peer_count].last_seen = time(NULL);

    peer_count++;
    printf("New peer discovered: %s\n", ip);
  }

  pthread_mutex_unlock(&peers_mutex);
}

void *handle_broadcast(void *arg) {
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
                             .sin_addr.s_addr = INADDR_ANY};

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind() failed");
    exit(EXIT_FAILURE);
  }

  // Create the response socket
  int response_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (response_sock < 1) {
    perror("socket() failed");
    exit(EXIT_FAILURE);
  }

  while (1) {
    char buffer[1024];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    // Receive broadcast message
    ssize_t recv_len = recvfrom(sock, buffer, sizeof(buffer), 0,
                                (struct sockaddr *)&sender_addr, &addr_len);

    if (recv_len > 0) {
      char sender_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);

      // Don't respond to our own broadcasts
      if (is_own_ip(sender_ip) != 0) {
        // Send unicast response
        sender_addr.sin_port = htons(RESPONSE_PORT);
        const char *response = "PEER RESPONSE";
        sendto(response_sock, response, strlen(response), 0,
               (struct sockaddr *)&sender_addr, sizeof(sender_addr));

        // Update/add the peer who sent us the broadcast
        update_peer(sender_ip);
      }
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
                             .sin_addr.s_addr = inet_addr("255.255.255.255")};

  while (1) {
    const char *message = "PEER DISCOVERY";
    sendto(sock, message, strlen(message), 0, (struct sockaddr *)&addr,
           sizeof(addr));

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

  while (1) {
    char buffer[1024];
    struct sockaddr_in sender_addr;
    socklen_t addr_len = sizeof(sender_addr);

    ssize_t bytes_received =
        recvfrom(sock, buffer, sizeof(buffer), 0,
                 (struct sockaddr *)&sender_addr, &addr_len);

    if (bytes_received > 0) {
      char sender_ip[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &(sender_addr.sin_addr), sender_ip, INET_ADDRSTRLEN);
      update_peer(sender_ip);
    }
  }
}
