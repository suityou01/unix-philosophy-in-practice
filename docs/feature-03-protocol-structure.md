# Feature 03: Protocol Structure & Message Framing

## Overview

In this branch, we transform our simple protocol into a robust, versioned, and extensible communication system. We add protocol identification, versioning, message length fields, and proper validationâall essential elements of real-world network protocols.

## What We Built

- **Magic number**: Protocol identification to prevent mismatched communication
- **Version field**: Enables protocol evolution and backward compatibility
- **Length field**: Supports variable-length messages
- **Structured headers**: Common format for all message types
- **Validation**: Checks for protocol correctness before processing

## The Problem with Feature 02

Our previous protocol was too simple:
```c
// Old request - just 1 byte
typedef struct {
    uint8_t command;
} request_t;
```

**Issues:**
1. No way to identify if bytes are actually our protocol
2. No version negotiationâcan't evolve the protocol
3. Fixed-size onlyâcan't handle variable data
4. Easy to accidentally connect wrong client to wrong server
5. No protection against corrupted data

## Protocol Design Principles

### 1. Magic Numbers

A **magic number** is a constant value that identifies a protocol or file format.
```c
#define PROTOCOL_MAGIC 0x54494D45  // "TIME" in ASCII
```

**Why "TIME"?**
```
T = 0x54
I = 0x49
M = 0x4D
E = 0x45
```

When you see `54 49 4D 45` in a hex dump, you immediately know it's our TIME protocol.

**Real-world examples:**
- PNG images: `89 50 4E 47` (â°PNG)
- PDF files: `25 50 44 46` (%PDF)
- Java class: `CA FE BA BE` (CAFEBABE)
- ZIP files: `50 4B 03 04` (PK)

**Purpose:**
- Quickly identify protocol/format
- Detect corruption (magic number wrong = corrupted data)
- Prevent accidental mismatches (HTTP client talking to TIME server)

### 2. Version Numbers
```c
#define PROTOCOL_VERSION 1
```

**Why versioning matters:**

Imagine we release v2 with a new feature. How do we handle:
- Old clients talking to new servers?
- New clients talking to old servers?

**Versioning strategies:**

**Option A: Strict matching** (what we do now)
```c
if (header->version != PROTOCOL_VERSION) {
    return RESP_BAD_VERSION;
}
```

**Option B: Backward compatible**
```c
if (header->version < MIN_SUPPORTED_VERSION || 
    header->version > PROTOCOL_VERSION) {
    return RESP_BAD_VERSION;
}
// Handle based on version
```

**Option C: Feature negotiation**
```c
// Client: "I support v1, v2, v3"
// Server: "I support v2, v3, v4"
// Result: Use v3 (highest common)
```

For now, we use strict matching. Future versions could implement negotiation.

### 3. Length Fields
```c
uint16_t length;  // Total message length including header
```

**Why we need this:**

TCP is a **stream protocol**âit doesn't preserve message boundaries.

**Without length field:**
```
Client sends: [MSG1][MSG2]
Server reads: [MSG1M] [SG2]  â Partial reads!
```

**With length field:**
```
Client sends: [LEN=10][8 bytes][LEN=15][13 bytes]
Server reads: "First message is 10 bytes" â read exactly 10
              "Second message is 15 bytes" â read exactly 15
```

**Payload size calculation:**
```c
uint16_t msg_length = ntohs(header.length);
uint16_t payload_size = msg_length - sizeof(message_header_t);
```

### 4. Flexible Array Members
```c
typedef struct {
    message_header_t header;
    uint8_t payload[0];  // Flexible array member
} request_message_t;
```

**What is `uint8_t payload[0]`?**

This is a C99 feature called a **flexible array member**. It's a zero-length array at the end of a struct.

**How it works:**
```c
// Allocate variable-sized message
size_t payload_len = 100;
request_message_t *msg = malloc(sizeof(message_header_t) + payload_len);

// Access payload
msg->payload[0] = 0x42;
msg->payload[99] = 0xFF;
```

**Why not just use a pointer?**
```c
// This would require two allocations:
typedef struct {
    message_header_t header;
    uint8_t *payload;  // Pointer
} request_message_t;

msg = malloc(sizeof(request_message_t));
msg->payload = malloc(payload_len);  // Second allocation!
```

Flexible array members keep everything in one contiguous block.

**Note:** In our current code, we don't actually use the flexible array yet (GET_TIME has no payload), but we're prepared for future commands that need it.

## The New Message Header
```c
typedef struct {
    uint32_t magic;      // Protocol identification (4 bytes)
    uint8_t version;     // Protocol version (1 byte)
    uint16_t length;     // Total message length (2 bytes)
    uint8_t command;     // Command/response type (1 byte)
} __attribute__((packed)) message_header_t;
// Total: 8 bytes
```

**Wire format:**
```
Offset  Size  Field       Example Value
------  ----  -----       -------------
0-3     4     magic       0x54 0x49 0x4D 0x45  ("TIME")
4       1     version     0x01
5-6     2     length      0x00 0x08  (8 bytes = header only)
7       1     command     0x01  (GET_TIME)
```

## Helper Functions

### Initializing Request Headers
```c
static inline void init_request_header(message_header_t *header, 
                                       uint8_t command, 
                                       uint16_t payload_len) {
    header->magic = htonl(PROTOCOL_MAGIC);
    header->version = PROTOCOL_VERSION;
    header->length = htons(sizeof(message_header_t) + payload_len);
    header->command = command;
}
```

**Why `static inline`?**
- `static`: Function is local to this file (doesn't pollute namespace)
- `inline`: Compiler should try to inline it (optimization hint)

**Usage:**
```c
message_header_t header;
init_request_header(&header, CMD_GET_TIME, 0);
// header is now properly initialized with magic, version, length, command
```

### Validating Headers
```c
static inline int validate_header(message_header_t *header) {
    if (ntohl(header->magic) != PROTOCOL_MAGIC) {
        return RESP_BAD_MAGIC;
    }
    if (header->version != PROTOCOL_VERSION) {
        return RESP_BAD_VERSION;
    }
    return RESP_OK;
}
```

**Why validate?**

1. **Wrong protocol**: If an HTTP client connects, magic won't match
2. **Corruption**: Network errors might flip bits
3. **Version mismatch**: Old client, new server (or vice versa)

**Early rejection saves processing:**
```c
// Validate BEFORE parsing payload
int validation = validate_header(&header);
if (validation != RESP_OK) {
    send_error_response(client_fd, validation);
    return;  // Don't process further!
}
```

## Server Implementation Deep Dive

### Reading the Header
```c
message_header_t header;
bytes_read = read(client_fd, &header, sizeof(header));

if (bytes_read < (ssize_t)sizeof(header)) {
    printf("â Incomplete header received\n");
    close(client_fd);
    return;
}
```

**Always check return value!** TCP can deliver partial data.

### Validation Flow
```c
int validation = validate_header(&header);
if (validation != RESP_OK) {
    if (validation == RESP_BAD_MAGIC) {
        printf("â Bad magic: 0x%08x (expected 0x%08x)\n", 
               ntohl(header.magic), PROTOCOL_MAGIC);
    } else if (validation == RESP_BAD_VERSION) {
        printf("â Bad version: %d (expected %d)\n", 
               header.version, PROTOCOL_VERSION);
    }
    send_error_response(client_fd, validation);
    close(client_fd);
    return;
}
```

**Defensive programming:**
- Check magic number first (fastest check)
- Check version next
- Only then proceed to parse payload

### Reading Variable-Length Payloads
```c
uint16_t msg_length = ntohs(header.length);
uint16_t payload_size = msg_length - sizeof(message_header_t);

if (payload_size > 0) {
    char payload[BUFFER_SIZE];
    
    // Safety check: don't overflow buffer
    if (payload_size >= BUFFER_SIZE) {
        printf("â Payload too large: %d bytes\n", payload_size);
        send_error_response(client_fd, RESP_ERROR);
        close(client_fd);
        return;
    }
    
    // Read exact payload size
    bytes_read = read(client_fd, payload, payload_size);
    if (bytes_read < payload_size) {
        printf("â Incomplete payload\n");
        send_error_response(client_fd, RESP_ERROR);
        close(client_fd);
        return;
    }
}
```

**Key points:**
1. Calculate payload size from length field
2. **Always** validate size before allocation/reading
3. Read exactly the number of bytes specified
4. Check for incomplete reads

### Sending Error Responses
```c
void send_error_response(int client_fd, uint8_t error_code) {
    response_message_t response;
    
    init_request_header(&response.header, CMD_GET_TIME, 
                       sizeof(response) - sizeof(message_header_t));
    response.status = error_code;
    response.timestamp = 0;
    
    write(client_fd, &response, sizeof(response));
}
```

**Always respond, even on error!** The client is waiting. Silence is confusing.

## Client Implementation Deep Dive

### Building Requests
```c
request_message_t request;
init_request_header(&request.header, CMD_GET_TIME, 0);

// Send just the header (GET_TIME has no payload)
write(sock_fd, &request.header, sizeof(request.header));
```

**For GET_TIME:**
- Length = 8 (header only)
- Payload size = 0

**For future commands (e.g., SET_TIME):**
```c
// Hypothetical SET_TIME with payload
typedef struct {
    uint32_t new_timestamp;
} set_time_payload_t;

set_time_payload_t payload;
payload.new_timestamp = htonl(new_time);

init_request_header(&request.header, CMD_SET_TIME, sizeof(payload));
write(sock_fd, &request.header, sizeof(request.header));
write(sock_fd, &payload, sizeof(payload));
```

### Validating Responses
```c
// Read response
read(sock_fd, &response, sizeof(response));

// Validate response header
int validation = validate_header(&response.header);
if (validation != RESP_OK) {
    if (validation == RESP_BAD_MAGIC) {
        printf("â Server sent bad magic number\n");
    }
    // ...
    return -1;
}
```

**Defense in depth:** Validate server responses too. Don't trust anything!

## Protocol Evolution Example

**Current (v1):**
```c
// GET_TIME request
Header: [MAGIC][VERSION=1][LENGTH=8][CMD=0x01]
(no payload)

// Response
Header: [MAGIC][VERSION=1][LENGTH=13][CMD=0x01]
Body:   [STATUS][TIMESTAMP]
```

**Future (v2) - hypothetical:**
```c
// GET_TIME request with timezone
Header: [MAGIC][VERSION=2][LENGTH=12][CMD=0x01]
Body:   [TIMEZONE_LEN][TIMEZONE_STR]

// Response with timezone info
Header: [MAGIC][VERSION=2][LENGTH=20][CMD=0x01]
Body:   [STATUS][TIMESTAMP][TIMEZONE_OFFSET][DST_FLAG]
```

**Handling v1 and v2 simultaneously:**
```c
switch (header.version) {
    case 1:
        handle_get_time_v1(client_fd);
        break;
    case 2:
        handle_get_time_v2(client_fd);
        break;
    default:
        send_error_response(client_fd, RESP_BAD_VERSION);
}
```

## Wire Protocol Comparison

**Feature 02 (old):**
```
Request:  [01]           (1 byte)
Response: [00][67 89 AB CD]  (5 bytes)
```

**Feature 03 (new):**
```
Request:  [54 49 4D 45][01][00 08][01]  (8 bytes)
           ââ magic ââ  ver  len   cmd

Response: [54 49 4D 45][01][00 0D][01][00][67 89 AB CD]  (13 bytes)
           ââ magic ââ  ver  len   cmd  st âtimestampâ
```

**Overhead increased:**
- Request: 1 byte â 8 bytes (7 bytes overhead)
- Response: 5 bytes â 13 bytes (8 bytes overhead)

**Is this worth it?**

**YES!** The benefits far outweigh the cost:
- Protocol identification
- Version negotiation capability
- Variable-length support
- Better error detection
- Professional, extensible design

**In modern networks:**
- 7 bytes extra = 0.000056 seconds on 1 Mbps connection
- Essentially free compared to TCP/IP overhead (~40 bytes per packet)

## Testing Protocol Validation

### Test 1: Wrong Magic Number
```bash
# Send garbage data
echo "NOTTIME" | nc localhost 9999
```

Server should respond:
```
â Bad magic number: 0x4e4f5454 (expected 0x54494d45)
```

### Test 2: Wrong Version

Modify client to send version 99:
```c
header->version = 99;
```

Server should respond:
```
â Bad version: 99 (expected 1)
```

### Test 3: Corrupted Length

Modify length to be impossibly large:
```c
header->length = htons(65535);
```

Server should detect and reject.

### Test 4: Hex Dump Analysis
```bash
# Capture with tcpdump
sudo tcpdump -i lo port 9999 -X

# Or use Wireshark
wireshark
```

You should see:
```
0x0000:  5449 4d45 0100 0801  TIME....
         âmagicâ v  âlenâcmd
```

## Common Pitfalls

### 1. Forgetting Network Byte Order on New Fields
```c
// WRONG
header->magic = PROTOCOL_MAGIC;
header->length = msg_len;

// RIGHT
header->magic = htonl(PROTOCOL_MAGIC);
header->length = htons(msg_len);
```

**All multi-byte fields need conversion!**

### 2. Not Validating Buffer Sizes
```c
// DANGEROUS - buffer overflow!
char payload[256];
read(client_fd, payload, header.length);  // What if length > 256?

// SAFE
if (payload_size < sizeof(payload)) {
    read(client_fd, payload, payload_size);
} else {
    // Reject message
}
```

### 3. Ignoring Partial Reads
```c
// WRONG - assumes read() returns full amount
read(fd, &header, sizeof(header));

// RIGHT - check return value
ssize_t n = read(fd, &header, sizeof(header));
if (n < sizeof(header)) {
    // Handle incomplete read
}
```

### 4. Wrong Length Calculation
```c
// WRONG - length is total message size, not payload size
init_request_header(&header, CMD, payload_size);

// RIGHT - length includes header
init_request_header(&header, CMD, sizeof(response) - sizeof(header));
```

## Protocol Design Wisdom

### Keep It Simple (KISS)

Our protocol is simple:
- Fixed header (8 bytes)
- Optional variable payload
- Clear validation rules

**Don't over-engineer:**
- â Compression (adds complexity)
- â Encryption in protocol (use TLS layer)
- â Complex state machines
- â Simple, clear, testable

### Make Invalid States Unrepresentable
```c
// BAD - requires checking both fields
struct {
    int has_payload;
    int payload_length;
};

// GOOD - length field tells you everything
struct {
    uint16_t length;  // 0 = no payload
};
```

### Fail Fast, Fail Loud
```c
// Validate immediately
if (validation != RESP_OK) {
    send_error_response(client_fd, validation);
    close(client_fd);  // Don't try to continue
    return;
}
```

**Don't limp along with bad data!**

## Real-World Protocol Comparison

### HTTP/1.1 Headers
```
GET /index.html HTTP/1.1
Host: example.com
Connection: keep-alive
```

**Similarities:**
- Version field (`HTTP/1.1`)
- Command field (`GET`)
- Length implied by `Content-Length` header

**Differences:**
- Text-based (we're binary)
- Much more complex parsing
- More human-readable

### DNS Protocol
```
[ID][FLAGS][QDCOUNT][ANCOUNT][NSCOUNT][ARCOUNT]
[QUESTION]
[ANSWER]
[AUTHORITY]
[ADDITIONAL]
```

**Similarities:**
- Binary protocol
- Fixed header size
- Length/count fields
- Request/response model

**Differences:**
- More complex structure
- Multiple sections
- Compression for names

### Our Protocol (TIME)

**We're closer to DNS than HTTP:**
- Binary (efficient)
- Fixed header (fast parsing)
- Simple structure (easy to implement)
- Length fields (handle variable data)

## What We've Achieved

â **Protocol identification** - magic number prevents confusion  
â **Versioning** - can evolve protocol over time  
â **Variable-length messages** - not limited to fixed sizes  
â **Robust validation** - detect errors early  
â **Professional structure** - follows industry best practices  
â **Extensibility** - easy to add new commands  

## What's Next

In `feature/04-set-time-unsafe`, we'll:
- Add SET_TIME command with payload
- Demonstrate the security vulnerability of no authentication
- Show why authorization matters
- Set up for proper security in later features

## Exercises

1. **Add a PING command** (0x03):
   - No payload
   - Response with just OK status, timestamp = 0
   
2. **Implement protocol version 2**:
   - Add a "client name" field to requests
   - Make server log client names
   
3. **Add message checksums**:
   - Include CRC32 in header
   - Validate checksum on receive
   
4. **Create a hexdump utility**:
   - Print all bytes sent/received
   - Annotate with field meanings
   
5. **Test with Wireshark**:
   - Capture packets
   - Verify byte order
   - Identify magic number
   
6. **Implement backward compatibility**:
   - Support v1 and v2 simultaneously
   - Gracefully handle version differences

## Further Reading

- RFC 2616 - HTTP/1.1 (see message format)
- RFC 1035 - DNS (binary protocol design)
- "The Practice of Programming" - Kernighan & Pike (Chapter 6: Testing)
- "TCP/IP Illustrated, Volume 1" - Stevens (protocol design principles)
- Magic numbers database: https://en.wikipedia.org/wiki/List_of_file_signatures

## Debugging Tips

**View raw protocol bytes:**
```bash
# Server side
./server | tee server.log

# Analyze
hexdump -C server.log
```

**Test with custom packets:**
```python
import socket
import struct

sock = socket.socket()
sock.connect(('localhost', 9999))

# Send valid request
magic = 0x54494D45
version = 1
length = 8
command = 0x01

packet = struct.pack('!IBHB', magic, version, length, command)
sock.send(packet)

response = sock.recv(1024)
print(response.hex())
```

**Verify byte order:**
```c
printf("Magic (host):    0x%08x\n", PROTOCOL_MAGIC);
printf("Magic (network): 0x%08x\n", htonl(PROTOCOL_MAGIC));
// Should be same on big-endian, swapped on little-endian
```

---

**Congratulations!** You've built a professional-grade protocol with proper structure, validation, and extensibility. This is how real protocols like DNS, NTP, and custom industrial protocols are designed.
