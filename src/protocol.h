#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define TIMESERVER_PORT 9999
#define BUFFER_SIZE 1024

// Protocol commands
#define CMD_GET_TIME 0x01
#define CMD_SET_TIME 0x02

// Response codes
#define RESP_OK 0x00
#define RESP_ERROR 0x01
#define RESP_UNAUTHORIZED 0x02

// Request message structure
typedef struct {
    uint8_t command;
} __attribute__((packed)) request_t;

// Response message structure
typedef struct {
    uint8_t status;
    uint32_t timestamp;
} __attribute__((packed)) response_t;

#endif // PROTOCOL_H
