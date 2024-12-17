#include "protocol.h"
#include <string.h>
#include <time.h>

static const char charset[] = "0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz";

void generate_token(char *token) {
  srand(time(NULL));
  int mod = sizeof(charset) - 1;

  for (int i = 0; i < TOKEN_LENGTH; i++) {
    int idx = rand() % mod;
    token[i] = charset[idx];
  }
  token[TOKEN_LENGTH] = '\0';
}

size_t serialize_message(const struct PeerMessage *msg, uint8_t *buffer) {
  // First byte: message header
  buffer[0] = (msg->header.version << 4) | (msg->header.is_response << 3) |
              (msg->header.flags);

  // Second byte: total message length
  buffer[1] = msg->length;

  // The token (10 bytes)
  memcpy(buffer + 2, msg->token, TOKEN_LENGTH);

  // Username length (1 byte)
  buffer[2 + TOKEN_LENGTH] = msg->username_length;

  // Username
  memcpy(buffer + TOKEN_LENGTH + 3, msg->username, msg->username_length);

  return msg->length;
}

int deserialize_message(const uint8_t *buffer, struct PeerMessage *msg) {
  // Extract header fields
  msg->header.version = (buffer[0] >> 4) & 0x0F;
  msg->header.is_response = (buffer[0] >> 3) & 0x01;
  msg->header.flags = buffer[0] & 0x7;

  // Verify protocol version
  if (msg->header.version != PROTOCOL_VERSION) {
    return -1;
  }

  // Get message length
  msg->length = buffer[1];

  // Get the token
  memcpy(msg->token, buffer + 2, TOKEN_LENGTH);
  msg->token[TOKEN_LENGTH] = '\0';

  // Get the username length
  msg->username_length = buffer[2 + TOKEN_LENGTH];

  // Verify the username length isn't over the max
  if (msg->username_length > MAX_USERNAME_LENGTH) {
    return -2;
  }

  // Get the username
  memcpy(msg->username, buffer + 3 + TOKEN_LENGTH, msg->username_length);
  msg->username[msg->username_length] = '\0';

  return 0;
}

uint8_t calculate_message_length(struct PeerMessage *msg) {
  // 1 byte header + 1 byte length + TOKEN_LENGTH + 1 byte username length +
  // username
  return 3 + TOKEN_LENGTH + msg->username_length;
}
