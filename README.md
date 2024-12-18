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
  - 1 byte to encode the username length in bytes (maximum of 255).
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



## Reference

- [Beej's Guide to Networking](https://beej.us/guide/bgnet/html/split/index.html)
- [Hands-On Network Programming in C](https://www.amazon.com/Hands-Network-Programming-programming-optimized/dp/1789349869)
- [socket 2 manpage](https://man7.org/linux/man-pages/man2/socket.2.html)
- [bind 2 manpage](https://man7.org/linux/man-pages/man2/bind.2.html)
- [setsockopt 3 manpage](https://man7.org/linux/man-pages/man3/setsockopt.3p.html)
