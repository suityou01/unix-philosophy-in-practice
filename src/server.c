#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

void send_error_response(int client_fd, uint8_t error_code) {
    response_message_t response;
    
    init_request_header(&response.header, CMD_GET_TIME, sizeof(response) - sizeof(message_header_t));
    response.status = error_code;
    response.timestamp = 0;
    
    write(client_fd, &response, sizeof(response));
}

void handle_get_time(int client_fd) {
    response_message_t response;
    
    // Get current Unix timestamp
    time_t current_time = time(NULL);
    
    // Build response header
    init_request_header(&response.header, CMD_GET_TIME, sizeof(response) - sizeof(message_header_t));
    
    // Build response body
    response.status = RESP_OK;
    response.timestamp = htonl((uint32_t)current_time);
    
    // Send response
    ssize_t sent = write(client_fd, &response, sizeof(response));
    if (sent < 0) {
        perror("write() failed");
    } else {
        printf("✓ Sent timestamp: %u\n", (uint32_t)current_time);
    }
}

void handle_client(int client_fd) {
    message_header_t header;
    ssize_t bytes_read;
    
    // Read message header
    bytes_read = read(client_fd, &header, sizeof(header));
    
    if (bytes_read < 0) {
        perror("read() failed");
        close(client_fd);
        return;
    }
    
    if (bytes_read == 0) {
        printf("Client disconnected\n");
        close(client_fd);
        return;
    }
    
    if (bytes_read < (ssize_t)sizeof(header)) {
        printf("✗ Incomplete header received (%zd bytes)\n", bytes_read);
        close(client_fd);
        return;
    }
    
    // Validate header
    int validation = validate_header(&header);
    if (validation != RESP_OK) {
        if (validation == RESP_BAD_MAGIC) {
            printf("✗ Bad magic number: 0x%08x (expected 0x%08x)\n", 
                   ntohl(header.magic), PROTOCOL_MAGIC);
        } else if (validation == RESP_BAD_VERSION) {
            printf("✗ Bad version: %d (expected %d)\n", 
                   header.version, PROTOCOL_VERSION);
        }
        send_error_response(client_fd, validation);
        close(client_fd);
        return;
    }
    
    // Get message details
    uint16_t msg_length = ntohs(header.length);
    uint8_t command = header.command;
    
    printf("Received valid message:\n");
    printf("  Magic: 0x%08x\n", ntohl(header.magic));
    printf("  Version: %d\n", header.version);
    printf("  Length: %d bytes\n", msg_length);
    printf("  Command: 0x%02x\n", command);
    
    // Calculate payload size
    uint16_t payload_size = msg_length - sizeof(message_header_t);
    
    // Read payload if present (for future commands)
    if (payload_size > 0) {
        char payload[BUFFER_SIZE];
        if (payload_size < BUFFER_SIZE) {
            bytes_read = read(client_fd, payload, payload_size);
            if (bytes_read < payload_size) {
                printf("✗ Incomplete payload\n");
                send_error_response(client_fd, RESP_ERROR);
                close(client_fd);
                return;
            }
        } else {
            printf("✗ Payload too large: %d bytes\n", payload_size);
            send_error_response(client_fd, RESP_ERROR);
            close(client_fd);
            return;
        }
    }
    
    // Handle command
    switch (command) {
        case CMD_GET_TIME:
            handle_get_time(client_fd);
            break;
        
        default:
            printf("✗ Unknown command: 0x%02x\n", command);
            send_error_response(client_fd, RESP_ERROR);
            break;
    }
    
    close(client_fd);
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    (void)argc;
    (void)argv;
    
    printf("Bell Labs Style Time Server v0.3\n");
    printf("=================================\n");
    printf("Protocol: TIME v%d (Magic: 0x%08x)\n", PROTOCOL_VERSION, PROTOCOL_MAGIC);
    printf("\n");
    printf("Supported commands:\n");
    printf("  0x01 - GET_TIME: Query system time\n");
    printf("\n");
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket() failed");
        exit(1);
    }
    
    // Allow port reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind to port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(TIMESERVER_PORT);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(server_fd);
        exit(1);
    }
    
    // Listen for connections
    if (listen(server_fd, 5) < 0) {
        perror("listen() failed");
        close(server_fd);
        exit(1);
    }
    
    printf("Server listening on port %d\n", TIMESERVER_PORT);
    printf("Press Ctrl+C to stop\n\n");
    
    // Accept loop
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept() failed");
            continue;
        }
        
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("Client connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        handle_client(client_fd);
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    }
    
    close(server_fd);
    return 0;
}
