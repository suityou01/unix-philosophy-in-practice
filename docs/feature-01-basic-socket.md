# Feature 01: Basic Socket Programming

## Overview

This branch implements the foundational building blocks of network programming: a TCP server that listens for connections and a client that connects to it. We're not implementing any protocol yet—just establishing the communication channel.

## What We Built

- **server.c**: A TCP server that accepts connections and echoes data back
- **client.c**: A TCP client that connects and sends a test message
- **protocol.h**: Shared constants (port number, buffer size)

## Core Concepts

### 1. Sockets: The Unix Way

In Unix, "everything is a file." A socket is just a special file descriptor that represents a network connection. You interact with it using the same system calls as files: `read()`, `write()`, `close()`.
```c
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
```

- `AF_INET`: Address Family - IPv4
- `SOCK_STREAM`: TCP (connection-oriented, reliable)
- `0`: Let the system choose the protocol (TCP for STREAM)

### 2. The Server Lifecycle

A TCP server follows this pattern:
```
socket() → bind() → listen() → accept() → read()/write() → close()
```

#### Step 1: Create Socket
```c
server_fd = socket(AF_INET, SOCK_STREAM, 0);
```
Creates an endpoint for communication. Returns a file descriptor.

#### Step 2: Bind to Address
```c
struct sockaddr_in server_addr;
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces
server_addr.sin_port = htons(TIMESERVER_PORT);

bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
```

Assigns the socket to a specific port. `INADDR_ANY` means "listen on all network interfaces."

**Why `htons()`?** Network byte order is big-endian, but your CPU might be little-endian. `htons()` (host-to-network-short) converts the port number to network byte order.

#### Step 3: Listen
```c
listen(server_fd, 5);
```

Marks the socket as passive (ready to accept connections). The `5` is the backlog—how many pending connections can queue up.

#### Step 4: Accept Connections
```c
client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
```

Blocks until a client connects. Returns a *new* file descriptor for this specific client connection. The original `server_fd` continues listening for more clients.

#### Step 5: Communicate
```c
read(client_fd, buffer, sizeof(buffer));
write(client_fd, buffer, bytes_read);  // Echo back
```

Standard file I/O operations work on sockets!

#### Step 6: Close
```c
close(client_fd);  // Close this client connection
close(server_fd);  // Eventually, close the server socket
```

### 3. The Client Lifecycle

A TCP client follows this pattern:
```
socket() → connect() → write()/read() → close()
```
```c
// Create socket
sock_fd = socket(AF_INET, SOCK_STREAM, 0);

// Specify server address
struct sockaddr_in server_addr;
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(TIMESERVER_PORT);
inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

// Connect to server
connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

// Communicate
write(sock_fd, message, length);
read(sock_fd, buffer, sizeof(buffer));

// Clean up
close(sock_fd);
```

**`inet_pton()`**: Converts IP address from text ("127.0.0.1") to binary network format.

### 4. Socket Options
```c
int opt = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

`SO_REUSEADDR` allows immediate rebinding to a port after the server stops. Without this, you'd get "Address already in use" errors when restarting the server quickly.

**Why?** TCP connections have a TIME_WAIT state that lingers after close. This option lets you ignore that.

## Running the Code

**Terminal 1 (Server):**
```bash
./server
```

**Terminal 2 (Client):**
```bash
./client
```

**Expected Output:**

Server:
```
Bell Labs Style Time Server v0.1
=================================

Server listening on port 9999
Press Ctrl+C to stop

Client connected from 127.0.0.1:xxxxx
Received: Hello, timeserver!
```

Client:
```
Connected to 127.0.0.1:9999
Server response: Hello, timeserver!
```

## Testing with netcat

You can also test the server manually:
```bash
# Terminal 1
./server

# Terminal 2
nc localhost 9999
Hello from netcat!
^C
```

The server will echo back whatever you type.

## What's Missing?

This is just raw socket communication. We have:
- ✅ Network connection established
- ✅ Data transmission working
- ❌ No protocol (server doesn't understand commands)
- ❌ No structured messages
- ❌ No error handling
- ❌ Server handles one client at a time (blocks)

## Key Takeaways

1. **Sockets are file descriptors** - use familiar read/write operations
2. **Server socket vs client socket** - accept() creates a new fd for each client
3. **Byte order matters** - always use htons/ntohs for network data
4. **Blocking I/O** - accept() and read() wait until data arrives
5. **The Unix philosophy** - simple tools that do one thing well

## Next Steps

In `feature/02-get-time`, we'll:
- Define actual protocol commands
- Implement GET_TIME to return the system time
- Parse incoming messages properly
- Send structured responses

## Bell Labs Wisdom

> "The key to building a successful system is to make each program do one thing well, and to make programs work together." - Doug McIlroy

We've built the communication channel. Next, we give it meaning.

## Further Reading

- Stevens, "Unix Network Programming" (the bible of socket programming)
- `man 2 socket`, `man 2 bind`, `man 2 listen`, `man 2 accept`
- Beej's Guide to Network Programming (free online resource)

## Exercises

1. Modify the server to print the client's IP address and port
2. Make the server handle multiple messages from the same client (loop in handle_client)
3. Try connecting from a different machine on your network
4. Use Wireshark to capture the TCP handshake
5. What happens if you run two servers on the same port?
