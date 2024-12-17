#ifndef BROADCAST_H
#define BROADCAST_H

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// Network configuration
#define BROADCAST_PORT 9005
#define RESPONSE_PORT 9006
#define MAX_PEERS 10
#define DISCOVERY_INTERVAL 5

// Each of the discovered peers.
typedef struct {
  char ip[INET_ADDRSTRLEN];
  time_t last_seen;
} Peer;

// External declarations for global variables
extern Peer peers[MAX_PEERS];
extern int peer_count;
extern pthread_mutex_t peers_mutex;

// ----- Functions ----- //

/**
 * Updates or adds the peer to the peers list, if the maximum
 * number of peers hasn't been reached yet.
 */
void update_peer(const char *ip);

/**
 * Thread function to handle incoming broadcast messages.
 * Listens for peer discovery broadcasts and sends unicast responses.
 *
 * It creates a socket to receive broadcasts in the broadcast port,
 * and another one to send a response back, in the response port.
 */
void *handle_broadcast(void *arg);

/**
 * Thread function to periodically send broadcast messages.
 * It creates a socket to send broadcast messages on the broadcast port.
 */
void *send_broadcast(void *arg);

/**
 * Thread function to handle incoming unicast responses coming from peers
 * that respond to the broadcast message by announcing they heard the call
 * and want to be registered as a peer.
 */
void *handle_responses(void *arg);

#endif
