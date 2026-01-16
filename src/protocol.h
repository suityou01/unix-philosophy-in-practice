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

// Message structure (will evolve)
typedef struct {
    uint8_t command;
    uint32_t timestamp;
    char auth_token[32];
} timeserver_message_t;

#endif // PROTOCOL_H
