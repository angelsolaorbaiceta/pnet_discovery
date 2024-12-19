# Private Network Discovery

Learning how to use low-level sockets in C by writing a program that uses UDP broadcast messages to find computers that are running the same program and registering them as peers.

Build the program:

```sh
$ make
```

Then run it:

```sh
$ ./bin/pnet_broadcast
```

If you want to learn how to create your own networked program that discovers connected peers in a private network, read on!

## Tutorial

As a kid, I always marveled at computers talking to each other.
Many years later (something around 15), here I am, learning low-level networking using sockets and the C programming langugage.
In this project, I design a very simple binary protocol to be used over UDP conenctions. 
The protocol is used to broadcast a discovery message inside a private network and register every computer that responds to the call.
Let's start by defining the protocol.


### The protocol

Messages will be binary-encoded, and will have a maximum length of 255 bytes--although they should typically be much smaller.
The message is made of four parts:

1. A message header (1 byte):
  - 4 bits to encode the protocol version (currently `0001`).
  - 1 bit to indicate if the message is a broadcast (`0`) or a response to a broadcast (`1`).
  - 3 bits reserved for flats (currently `000`).
2. The total message length (1 byte).
2. A 10-byte alphanumeric token identifying a peer (10 bytes).
4. A n-byte username identifying a peer (`1 + n` bytes):
  - 1 byte to encode the username length in bytes (maximum of 100).
  - n bytes with the ASCII-encoded username.


Thus, a message's length is: `1 + 1 + 10 + (1 + n)` bytes, where `n` is the length of the username.

Example message:

```
0x10, 0x13, 0x50, 0x65, 0x43, 0x39, 0x6f, 0x43,
0x4c, 0x6a, 0x71, 0x71, 0x06, 0x68, 0x61, 0x63,
0x6b, 0x65, 0x72
```

Broken down into its parts:

```
// The header (0001 0000)
// Version = 0001
// It's a broadcast message (5th bit = 0)
// No flags (000)
0x10,

// The message length
// The message is 19 bytes long
0x13,

// The token
// "PeC9oCLjqq"
0x50, 0x65, 0x43, 0x39, 0x6f, 
0x43, 0x4c, 0x6a, 0x71, 0x71, 

// The username length
// The username is 6 bytes long
0x06, 

// The username
// "hacker"
0x68, 0x61, 0x63, 0x6b, 0x65, 0x72
```

Let's write some code for the protocol.


### Coding the Protocol

Let's define some constants for the protocol in a _protocol.h_ header file:

```c
/* protocol.h */

// Protocol constants
#define PROTOCOL_VERSION 0x01
#define TOKEN_LENGTH 10
#define MAX_USERNAME_LENGTH 100
#define MAX_MESSAGE_LENGTH 255

// Message type
#define MESSAGE_TYPE_BROADCAST 0x00
#define MESSAGE_TYPE RESPONSE 0x01
```

Then, let's use a [bit field struct](https://learn.microsoft.com/en-us/cpp/c-language/c-bit-fields) to represent the protocol's data header:

```c
/* protocol.h */

struct ProtocolHeader {
  /** Protocol version (4 bits --> 0001). */
  unsigned int version : 4;
  /** Whether it's the broadcast message or response message (1 bit). */
  unsigned int is_response : 1;
  /** Reserved flags (3 bits). Unused for now. */
  unsigned int flags : 3;
};
```

And with it, let's define the protocol's data struct:

```c
/* protocol.h */

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
```

We now need a few functions--whose signature will be added to the header file--to serialize, deserialize and perform other operations to our protocol's data struct.
Let's start with a function to generate a random 10-byte alphanumeric token that each peer gets assigned on start-up.
The `generate_token()` function takes in a pointer to a string, and sets the 10 characters in it.
The caller should make sure enough space is allocated for it.

```c
/* protocol.c */

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
```

Nothing surprising about that function (except for ending the token with a null character `\0` if you are used to higher-level programming languages that provide the `string` abstraction).

Let's write a `serialize_message()` function that takes a pointer to a `struct PeerMessage` and a pointer to a `uint8_t` buffer, and serializes the message into the buffer.
The serialization of the header requires some bit manipulation:

- The version is left-shifted 4 bits (`header.version << 4`).
- The `is_response` field is left-shifted 3 bits (`header.is_response << 3`).

The caller of this function needs to make sure enough space is allocated in the `buffer` array.
Ideally, it should be exactly the length of the message (`msg->length`).
The function is as follows:

```c
/* protocol.c */

void serialize_message(const struct PeerMessage *msg, uint8_t *buffer) {
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
}
```

Now, we're going to need the reverse operation: `deserialize_message()`.
In this case, we'll return an integer that will be `0` if the deserialization process works correctly, and a negative number otherwise.
There are two reasons why the deserialization may fail, namely:

1. The header version doesn't match our current version--return `-1`.
2. The username is longer than the maximum length of 100 bytes--return `-2`.

Once again, we're trusting the client of the function to provide a buffer that's large enough to hold the message.
Here's the code:

```c
/* protocol.c */

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
```

Now, were going to need a function that calculates the size of a message, so we can include that information as part of the message itself.
Let's write a `calculate_message_length()` function for it.
As we know, the size of a message is 3 bytes (1 for the header, plus 1 for the message size, and 1 for the username length), plus the token length (10 bytes), plus the length of the username.

```c
/* protocol.c */

uint8_t calculate_message_length(struct PeerMessage *msg) {
  // 1 byte header + 1 byte length + TOKEN_LENGTH + 1 byte username length +
  // username
  return 3 + TOKEN_LENGTH + msg->username_length;
}
```

And last, it's going to be handy if we write a function that creates a broadcast message and serializes it.
Let's call this function `serialized_response()`.
We need to pass it the users token and name, as well as the buffer to serialize the message to.

```c
/* protocol.h */

int serialized_broadcast(char *token, char *username, uint8_t *buffer) {
  struct PeerMessage msg = {
      .header = {.version = PROTOCOL_VERSION, .is_response = 0, .flags = 0},
      .username_length = strlen(username)};

  strncpy(msg.token, token, TOKEN_LENGTH);
  strncpy(msg.username, username, msg.username_length);
  msg.username[msg.username_length] = '\0';
  msg.length = calculate_message_length(&msg);

  serialize_message(&msg, buffer);

  return msg.length;
}
```

Note the message's `is_response` field is set to `0`, signaling this message is a broadcast message.
The function returns the message's length, but keeps the message for itself.
This means that the message is allocated in the function's stack, and thus "deallocated" when the function returns.

Let's create a similar function, but to serialize response messages.
We'll--obviously--call it `serialized_response()`:

```c
/* protocol.c */

int serialized_response(char *token, char *username, uint8_t *buffer) {
  struct PeerMessage response = {
      .header = {.version = PROTOCOL_VERSION, .is_response = 1, .flags = 0},
      .username_length = strlen(username)};
  strncpy(response.token, token, TOKEN_LENGTH);
  strncpy(response.username, username, response.username_length);
  response.username[response.username_length] = '\0';
  response.length = calculate_message_length(&response);

  serialize_message(&response, buffer);

  return response.length;
}
```

The only notable difference here is that the `is_response` field is set to `1` in this occasion, signaling this is a response message.

Let's focus on sending the messages over a UDP connection.


### Broadcast and response UDP messages

Let's start by declaring the peers array--with size `MAX_PEERS`--, the peers count, and a mutex to synchronize access to the peers array.
There'll be different threads in our program, so we need a means of synchronization.

```c
/* broadcast.c */

// Initialize global variables
Peer peers[MAX_PEERS];
int peer_count = 0;
pthread_mutex_t peers_mutex = PTHREAD_MUTEX_INITIALIZER;
```

Next, lets declare two arrays: one for the user token and another for the username.

```c
/* broadcast.c */

// Our identity info
char my_token[TOKEN_LENGTH + 1];
char my_username[MAX_USERNAME_LENGTH + 1];
```

Let's write a function to initialize these two pieces of information when the program starts.
We'll call it `init_my_info()`.
Generating the token is straightforward using the function we wrote earlier: `generate_token()`.
To get the username, we use the `getpwuid()` function call, wich reads the _/etc/passwd_ file and returns the fields for a given user id.

```c
/* broadcast.c */

void init_my_info(void) {
  generate_token(my_token);

  struct passwd *pw = getpwuid(getuid());
  if (pw) {
    strcpy(my_username, pw->pw_name);
  } else {
    strcpy(my_username, "Unknown");
  }
}
```

Similarly, a function to initialize a `Peer` struct will be handy:

```c
/* broadcast.c */

void init_peer(Peer *peer, const char *ip, const char *token,
               const char *username) {
  strncpy(peer->ip, ip, INET_ADDRSTRLEN);
  peer->ip[INET_ADDRSTRLEN - 1] = '\0';

  strncpy(peer->token, token, TOKEN_LENGTH);
  peer->token[TOKEN_LENGTH] = '\0';

  strncpy(peer->username, username, MAX_USERNAME_LENGTH);
  peer->username[MAX_USERNAME_LENGTH] = '\0';

  peer->last_seen = time(NULL);
}
```

Now, let's get to the meat of the implementation.
We'll first implement a function to add or update a peer as their response message is received.
This function, let's call it `update_peer()` receives an IP, a token and a username and:

1. Looks in the peers array to check if a peer with such a token exists.
2. If it exists, we update its last seen timestamp and its IP and username, in case they have changed.
3. If the peer wasn't registered, it checks if there's room for one more peer, and adds it into the array.

Here's the implementation:

```c
/* broadcast.c */

void update_peer(const char *ip, const char *token, const char *username) {
  pthread_mutex_lock(&peers_mutex);

  // Check if peer already exists by token.
  // Update the last_seen, IP and username if so.
  int found = 0;
  for (int i = 0; i < peer_count; i++) {
    if (strcmp(peers[i].token, token) == 0) {
      peers[i].last_seen = time(NULL);

      // Check if the IP has changed
      if (strcmp(peers[i].ip, ip)) {
        strncpy(peers[i].ip, ip, INET_ADDRSTRLEN - 1);
        peers[i].ip[INET_ADDRSTRLEN - 1] = '\0';
      }

      // Check if the username has changed
      if (strcmp(peers[i].username, username)) {
        strncpy(peers[i].username, username, MAX_USERNAME_LENGTH);
        peers[i].username[MAX_USERNAME_LENGTH] = '\0';
      }

      found = 1;
      break;
    }
  }

  // If the peer is new, add it if there is space
  if (!found && peer_count < MAX_PEERS) {
    init_peer(&peers[peer_count], ip, token, username);
    peer_count++;
  }

  pthread_mutex_unlock(&peers_mutex);
}

```

## Reference

- [Beej's Guide to Networking](https://beej.us/guide/bgnet/html/split/index.html)
- [Hands-On Network Programming in C](https://www.amazon.com/Hands-Network-Programming-programming-optimized/dp/1789349869)
- [socket 2 manpage](https://man7.org/linux/man-pages/man2/socket.2.html)
- [bind 2 manpage](https://man7.org/linux/man-pages/man2/bind.2.html)
- [setsockopt 3 manpage](https://man7.org/linux/man-pages/man3/setsockopt.3p.html)
