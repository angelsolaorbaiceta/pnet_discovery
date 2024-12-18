#include "netutils.h"
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>

int bind_broadcast_socket(int port) {
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    perror("socket() failed");
    return -1;
  }

  // Enable broadcast
  int broadcastEnabled = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnabled,
                 sizeof(broadcastEnabled)) < 0) {
    perror("setsockopt() failed");
    return -1;
  }

  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(port),
                             .sin_addr.s_addr = INADDR_ANY};

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind() failed");
    return -1;
  }

  return sock;
}
