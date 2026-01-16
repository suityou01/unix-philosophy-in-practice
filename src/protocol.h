#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define TIMESERVER_PORT 9999
#define BUFFER_SIZE 1024

// Protocol magic number (identifies our protocol)
#define PROTOCOL_MAGIC 0x54494D45  // "TIME" in ASCII

// Protocol version
#define PROTOCOL_VERSION 1

// Protocol commands
#define CMD_GET_TIME 0x01
#define CMD_SET_TIME 0x02

// Response codes
#define RESP_OK 0x00
#define RESP_ERROR 0x01
#define RESP_UNAUTHORIZED 0x02
#define RESP_BAD_VERSION 0x03
#define RESP_BAD_MAGIC 0x04

// Message header (common to all messages)
typedef struct {
    uint32_t magic;      // Protocol identification
    uint8_t version;     // Protocol version
    uint16_t length;     // Total message length including header
    uint8_t command;     // Command or response type
} __attribute__((packed)) message_header_t;

// Request message structure
typedef struct {
    message_header_t header;
    uint8_t payload[0];  // Flexible array member for future use
} __attribute__((packed)) request_message_t;

// GET_TIME has no payload

// Response message structure
typedef struct {
    message_header_t header;
    uint8_t status;
    uint32_t timestamp;
} __attribute__((packed)) response_message_t;

// Helper function to create request header
static inline void init_request_header(message_header_t *header, uint8_t command, uint16_t payload_len) {
    header->magic = htonl(PROTOCOL_MAGIC);
    header->version = PROTOCOL_VERSION;
    header->length = htons(sizeof(message_header_t) + payload_len);
    header->command = command;
}

// Helper function to validate header
static inline int validate_header(message_header_t *header) {
    if (ntohl(header->magic) != PROTOCOL_MAGIC) {
        return RESP_BAD_MAGIC;
    }
    if (header->version != PROTOCOL_VERSION) {
        return RESP_BAD_VERSION;
    }
    return RESP_OK;
}

#endif // PROTOCOL_H
