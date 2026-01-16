#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

void handle_get_time(int client_fd) {
    response_t response;
    
    // Get current Unix timestamp
    time_t current_time = time(NULL);
    
    // Build response
    response.status = RESP_OK;
    response.timestamp = htonl((uint32_t)current_time);  // Convert to network byte order
    
    // Send response
    ssize_t sent = write(client_fd, &response, sizeof(response));
    if (sent < 0) {
        perror("write() failed");
    } else {
        printf("Sent timestamp: %u\n", (uint32_t)current_time);
    }
}

void handle_client(int client_fd) {
    request_t request;
    ssize_t bytes_read;
    
    // Read request from client
    bytes_read = read(client_fd, &request, sizeof(request));
    
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
    
    if (bytes_read < (ssize_t)sizeof(request)) {
        printf("Incomplete request received\n");
        close(client_fd);
        return;
    }
    
    // Handle command
    printf("Received command: 0x%02x\n", request.command);
    
    switch (request.command) {
        case CMD_GET_TIME:
            handle_get_time(client_fd);
            break;
        
        default:
            printf("Unknown command: 0x%02x\n", request.command);
            response_t error_response;
            error_response.status = RESP_ERROR;
            error_response.timestamp = 0;
            write(client_fd, &error_response, sizeof(error_response));
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
    
    printf("Bell Labs Style Time Server v0.2\n");
    printf("=================================\n");
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
        
        printf("Client connected from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        handle_client(client_fd);
    }
    
    close(server_fd);
    return 0;
}
