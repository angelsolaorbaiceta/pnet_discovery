#ifndef PEER_PROTOCOL_H
#define PEER_PROTOCOL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Protocol constants
#define PROTOCOL_VERSION 0x01
#define TOKEN_LENGTH 10
#define MAX_USERNAME_LENGTH 100
#define MAX_MESSAGE_LENGTH 255

// Message type
#define MESSAGE_TYPE_BROADCAST 0x00
#define MESSAGE_TYPE RESPONSE 0x01

/**
 * The protocol's header (first byte of the message).
 * Defined as a bit field (See:
 * https://learn.microsoft.com/en-us/cpp/c-language/c-bit-fields).
 *
 * The byte of the header is distributed as follows:
 *
 *  - 4 bits for the protocol version.
 *  - 1 bit for the message type (broadcast or response).
 *  - 3 bits for the flags (reserved for future use).
 */
struct ProtocolHeader {
  /** Protocol version (4 bits --> 0001). */
  unsigned int version : 4;
  /** Whether it's the broadcast message or response message (1 bit). */
  unsigned int is_response : 1;
  /** Reserved flags (3 bits). Unused for now. */
  unsigned int flags : 3;
};

/**
 * The protocol's complete message. The message includes:
 *
 *  - The protocol header.
 *  - The total length of the message in bytes.
 *  - The unique user's token.
 *  - The length of the username.
 *  - The username.
 */
struct PeerMessage {
  /** Message header. */
  struct ProtocolHeader header;
  /** Message total size in bytes. */
  uint8_t length;
  /**
   * Unique token assigned to each peer at startup. Maintained throughout the
   * session.
   */
  char token[TOKEN_LENGTH + 1];
  /** The length of the username of the peer. */
  uint8_t username_length;
  /** The user name of the peer's computer. */
  char username[MAX_USERNAME_LENGTH + 1];
};

// ----- Functions ----- //

/**
 * Generates a 10 byte random token consisting of alphanumeric characters.
 * This token should remain unchanged during the entire user session.
 * Other peers will uniquely identify the peer by its unique token, even
 * when the IP address of the peer changes.
 */
void generate_token(char *token);

/**
 * Serializes the message to bytes and stores the result in the passed in
 * buffer.
 */
void serialize_message(const struct PeerMessage *msg, uint8_t *buffer);

/**
 * Deserializes the bytes in the buffer into a message.
 * Returns 0 if everything goes right.
 * Otherwise, it returns:
 *
 *  - -1: invalid protocol version.
 *  - -2: invalid username length.
 */
int deserialize_message(const uint8_t *buffer, struct PeerMessage *msg);

uint8_t calculate_message_length(struct PeerMessage *msg);

/**
 * Creates the response message and serializes it into the buffer.
 *
 * Returns the message length if all goes well, and a negative number (see
 * `deserialize_message`) if there's an error in the serialization process.
 */
int serialized_response(char *token, char *username, uint8_t *buffer);

/**
 * Creates the broadcast message and serializes it into the buffer.
 * The length of the messge is returned.
 */
int serialized_broadcast(char *token, char *username, uint8_t *buffer);

#endif
