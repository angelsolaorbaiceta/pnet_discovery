#ifndef BROADCAST_H
#define BROADCAST_H

#include "netutils.h"
#include "protocol.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// Network configuration
#define BROADCAST_PORT 9005
#define RESPONSE_PORT 9006
#define DISCOVERY_INTERVAL 5
#define STALE_PEER_TIMEOUT 15
#define BROADCAST_IP "255.255.255.255"

/**
 * Each of the discovered peers.
 * A peer has an IPv4 address (typically in the 192.169.0.0/16 subnet),
 * a unique token, a username and a timestamp of when it was last seen.
 *
 * Peers are stored in a linked list, and so there's a next pointer,
 * pointing to the next peer.
 */
typedef struct Peer {
  char ip[INET_ADDRSTRLEN];
  char token[TOKEN_LENGTH + 1];
  char username[MAX_USERNAME_LENGTH + 1];
  time_t last_seen;

  struct Peer *next;
} Peer;

// External declarations for global variables
extern Peer *peers;
extern int peer_count;
extern pthread_mutex_t peers_mutex;
extern char my_token[TOKEN_LENGTH + 1];
extern char my_username[MAX_USERNAME_LENGTH + 1];

// ----- Functions ----- //

/**
 * Inits the token with random alphanumeric characters and sets the computer
 * user name as the program's username.
 */
void init_my_info(void);

/**
 * Initializes the peer struct instance, setting its IP address, identifier
 * token and name.
 */
void init_peer(Peer *peer, const char *ip, const char *token,
               const char *username);

/**
 * Updates or adds the peer to the peers list.
 * Peers are identified by their unique token, so even when their IP changes,
 * they can still be identified as being the same.
 *
 * It can only add the peer if the maximum number of peers hasn't been reached
 * yet.
 */
void update_peer(const char *ip, const char *token, const char *username);

/**
 * Removes those peers that have been disconnected for longer than
 * STALE_PEER_TIMEOUT.
 */
void remove_stale_peers();

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
