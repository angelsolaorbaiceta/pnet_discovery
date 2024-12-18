#ifndef NETUTILS_H
#define NETUTILS_H

/**
 * Creates a broadcast UDP socket and binds it to the given port at localhost.
 */
int bind_broadcast_socket(int port);

#endif
