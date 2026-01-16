# Feature 02: GET_TIME - Implementing Our First Protocol

## Overview

In this branch, we move beyond raw socket communication to implement an actual protocol. The server can now understand commands and respond with structured data. We've implemented the GET_TIME command, which returns the current Unix timestamp.

## What We Built

- **Binary protocol**: Structured request/response messages
- **GET_TIME command**: Query the server's system time
- **Network serialization**: Converting data structures to bytes
- **Command parsing**: Server interprets client requests

## The Unix Timestamp

Before diving into the code, let's understand what we're transmitting:
```c
time_t current_time = time(NULL);
```

This returns the **Unix epoch timestamp**: seconds since January 1, 1970, 00:00:00 UTC. It's a universal time representation used across all Unix systems.

**Example:**
- `1737039840` = January 16, 2026, 15:30:40 UTC

**Why Unix time?**
- Simple: just one number
- Universal: no timezone complications
- Easy to calculate with: time differences are just subtraction
- Compact: fits in 32 bits (until 2038... but that's a different problem!)

## Protocol Design

### Request Structure
```c
typedef struct {
    uint8_t command;
} __attribute__((packed)) request_t;
```

**Fields:**
- `command`: 1 byte indicating what action to perform
  - `0x01` = GET_TIME
  - `0x02` = SET_TIME (coming in future branches)

**Size:** 1 byte total

### Response Structure
```c
typedef struct {
    uint8_t status;
    uint32_t timestamp;
} __attribute__((packed)) response_t;
```

**Fields:**
- `status`: 1 byte indicating success/error
  - `0x00` = OK
  - `0x01` = ERROR
  - `0x02` = UNAUTHORIZED (for future use)
- `timestamp`: 4 bytes containing Unix epoch time

**Size:** 5 bytes total

### The `__attribute__((packed))` Directive

This is crucial! Without it, the compiler might add padding:
```c
// Without packed:
struct {
    uint8_t status;    // 1 byte
    // [3 bytes padding added here for alignment!]
    uint32_t timestamp; // 4 bytes
}; // Total: 8 bytes

// With packed:
struct {
    uint8_t status;     // 1 byte
    uint32_t timestamp; // 4 bytes
} __attribute__((packed)); // Total: 5 bytes
```

**Why does this matter?** We're sending raw bytes over the network. Both sides must agree on the exact byte layout. Padding would break this agreement.

## Network Byte Order (Endianness)

Different CPUs store multi-byte numbers differently:

**Little-Endian (x86, x86_64, ARM):**
```
Number: 0x12345678
Memory: [78] [56] [34] [12]  (least significant byte first)
```

**Big-Endian (Network standard):**
```
Number: 0x12345678
Memory: [12] [34] [56] [78]  (most significant byte first)
```

**The Problem:** If a little-endian client sends `0x12345678` directly, a big-endian server reads `0x78563412`!

**The Solution:** Network byte order is ALWAYS big-endian. Use conversion functions:
```c
// Host to Network (before sending)
uint32_t network_time = htonl(local_time);  // htonl = host-to-network-long

// Network to Host (after receiving)
uint32_t local_time = ntohl(network_time);  // ntohl = network-to-host-long
```

**Available functions:**
- `htons()` / `ntohs()`: 16-bit values (short)
- `htonl()` / `ntohl()`: 32-bit values (long)
- `htonll()` / `ntohll()`: 64-bit values (long long) - not standard, use with caution

**On the wire, our response looks like:**
```
Offset  Value       Description
------  -----       -----------
0       0x00        Status (RESP_OK)
1-4     0x6789ABCD  Timestamp in network byte order
```

## Server Implementation Deep Dive

### The Main Handler
```c
void handle_client(int client_fd) {
    request_t request;
    
    // Read exactly sizeof(request_t) bytes
    ssize_t bytes_read = read(client_fd, &request, sizeof(request));
    
    // Validate we got complete message
    if (bytes_read < (ssize_t)sizeof(request)) {
        printf("Incomplete request\n");
        close(client_fd);
        return;
    }
    
    // Dispatch to handler
    switch (request.command) {
        case CMD_GET_TIME:
            handle_get_time(client_fd);
            break;
        
        default:
            // Send error response
            response_t error_response;
            error_response.status = RESP_ERROR;
            error_response.timestamp = 0;
            write(client_fd, &error_response, sizeof(error_response));
            break;
    }
    
    close(client_fd);
}
```

**Key points:**
1. Read exact number of bytes expected
2. Validate message completeness
3. Dispatch based on command byte
4. Always send a response (even for errors)
5. Close connection after handling

### GET_TIME Implementation
```c
void handle_get_time(int client_fd) {
    response_t response;
    
    // Get system time
    time_t current_time = time(NULL);
    
    // Build response
    response.status = RESP_OK;
    response.timestamp = htonl((uint32_t)current_time);  // Convert to network order!
    
    // Send response
    write(client_fd, &response, sizeof(response));
}
```

**The `time()` system call:**
```c
#include <time.h>
time_t time(time_t *tloc);
```

Returns current time as seconds since epoch. Passing `NULL` is common - we don't need the value stored in a pointer.

## Client Implementation Deep Dive

### Sending the Request
```c
request_t request;
request.command = CMD_GET_TIME;
write(sock_fd, &request, sizeof(request));
```

Simple! We're sending a single byte: `0x01`

### Receiving the Response
```c
response_t response;
ssize_t bytes = read(sock_fd, &response, sizeof(response));

if (bytes < (ssize_t)sizeof(response)) {
    printf("Incomplete response\n");
    return -1;
}

if (response.status == RESP_OK) {
    uint32_t timestamp = ntohl(response.timestamp);  // Convert from network order!
    print_time(timestamp);
}
```

**Critical steps:**
1. Read exactly the expected number of bytes
2. Validate we got complete response
3. Convert from network byte order
4. Check status before using data

### Pretty-Printing the Time
```c
void print_time(uint32_t timestamp) {
    time_t t = (time_t)timestamp;
    struct tm *tm_info = localtime(&t);  // Convert to local timezone
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);  // Format as string
    printf("Server time: %s (Unix: %u)\n", buffer, timestamp);
}
```

**`localtime()`** converts Unix timestamp to a `struct tm` with year, month, day, hour, etc.

**`strftime()`** formats that structure as a human-readable string.

## Testing Protocol with netcat

You can manually test the protocol:
```bash
# Terminal 1
./server

# Terminal 2 - Send raw bytes
printf '\x01' | nc localhost 9999 | xxd
```

Output (example):
```
00000000: 0067 89ab cd                             .g...
```

Breaking this down:
- `00` = RESP_OK
- `6789abcd` = timestamp in network byte order

## Wire Protocol Visualization

**Client sends (GET_TIME request):**
```
+--------+
| 0x01   |  Command: GET_TIME
+--------+
```

**Server responds:**
```
+--------+------------------+
| 0x00   | 0x6789ABCD      |
+--------+------------------+
  Status   Timestamp (network byte order)
```

## Common Pitfalls

### 1. Forgetting Network Byte Order
```c
// WRONG - sends in host byte order
response.timestamp = current_time;

// RIGHT - converts to network byte order
response.timestamp = htonl(current_time);
```

### 2. Partial Reads

TCP is a stream protocol - `read()` might return fewer bytes than requested!
```c
// Naive (might fail):
read(fd, &response, sizeof(response));

// Better (check return value):
ssize_t bytes = read(fd, &response, sizeof(response));
if (bytes < (ssize_t)sizeof(response)) {
    // Handle incomplete read
}

// Production (loop until complete):
size_t total = 0;
while (total < sizeof(response)) {
    ssize_t n = read(fd, ((char*)&response) + total, sizeof(response) - total);
    if (n <= 0) break;
    total += n;
}
```

### 3. Struct Padding
```c
// Without __attribute__((packed)), this might be 8 bytes instead of 5!
typedef struct {
    uint8_t status;
    uint32_t timestamp;
} response_t;
```

### 4. Signed vs Unsigned
```c
// WRONG - time_t might be signed
int32_t timestamp;

// RIGHT - Unix timestamps are always positive
uint32_t timestamp;
```

## What We've Achieved

✅ **Binary protocol** - compact, efficient  
✅ **Structured communication** - client and server speak same language  
✅ **Network serialization** - handling endianness correctly  
✅ **Command/response pattern** - foundation for more commands  
✅ **Error handling** - proper validation and error responses  

## What's Still Missing

❌ **Authentication** - anyone can query time  
❌ **Authorization** - no permission checking  
❌ **Multiple commands** - only GET_TIME implemented  
❌ **Concurrent clients** - server handles one at a time  
❌ **Persistent connections** - one request per connection  

## Bell Labs Lesson

> "Simplicity is prerequisite for reliability." - Edsger Dijkstra

Our protocol is deliberately simple:
- Fixed-size messages (easy to parse)
- Single command per connection (clear lifecycle)
- Minimal fields (no unnecessary complexity)

We can add complexity later. Start simple, prove it works, then extend.

## Testing Checklist

- [ ] Server starts and listens
- [ ] Client connects successfully
- [ ] GET_TIME returns reasonable timestamp
- [ ] Timestamp converts to correct date/time
- [ ] Server handles unknown commands gracefully
- [ ] Multiple sequential requests work
- [ ] Works across different machines (test endianness!)

## Exercises

1. **Add a PING command** (0x03) that just returns RESP_OK with timestamp = 0
2. **Modify client** to query time every second (loop)
3. **Calculate time difference** between client and server
4. **Test endianness**: Run server on one architecture, client on another
5. **Add logging**: Print hex dump of all bytes sent/received
6. **Implement timeout**: Client should give up if server doesn't respond in 5 seconds

## Next Steps

In `feature/03-protocol-structure`, we'll:
- Add more fields to our messages
- Implement proper message framing
- Handle variable-length data
- Prepare for the SET_TIME command

## Further Reading

- `man 2 time` - time() system call
- `man 3 strftime` - time formatting
- `man 3 localtime` - timezone conversion
- RFC 868 - Time Protocol (similar to what we built!)
- "TCP/IP Illustrated, Volume 1" - Stevens (Chapter 3: IP, covers byte order)

## Debugging Tips

**Print raw bytes received:**
```c
void hexdump(void *data, size_t len) {
    unsigned char *p = data;
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", p[i]);
    }
    printf("\n");
}

// Use it:
hexdump(&response, sizeof(response));
```

**Check endianness of your machine:**
```c
uint32_t test = 0x12345678;
unsigned char *p = (unsigned char *)&test;
if (p[0] == 0x78) {
    printf("Little-endian\n");
} else {
    printf("Big-endian\n");
}
```

**Verify network byte order conversion:**
```c
uint32_t local = 0x12345678;
uint32_t network = htonl(local);
printf("Local:   0x%08x\n", local);
printf("Network: 0x%08x\n", network);
printf("Back:    0x%08x\n", ntohl(network));
```

---

**Congratulations!** You've implemented your first network protocol. This is the foundation that protocols like HTTP, SSH, and DNS are built on. Same principles, just more complexity.
