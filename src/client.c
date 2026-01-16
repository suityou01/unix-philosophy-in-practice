#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

void print_time(uint32_t timestamp) {
    time_t t = (time_t)timestamp;
    struct tm *tm_info = localtime(&t);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("Server time: %s (Unix: %u)\n", buffer, timestamp);
}

int get_time(const char *server_ip) {
    int sock_fd;
    struct sockaddr_in server_addr;
    request_message_t request;
    response_message_t response;
    
    // Create socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket() failed");
        return -1;
    }
    
    // Connect to server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TIMESERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sock_fd);
        return -1;
    }
    
    printf("Connected to %s:%d\n", server_ip, TIMESERVER_PORT);
    
    // Build GET_TIME request
    init_request_header(&request.header, CMD_GET_TIME, 0);
    
    printf("Sending request:\n");
    printf("  Magic: 0x%08x\n", PROTOCOL_MAGIC);
    printf("  Version: %d\n", PROTOCOL_VERSION);
    printf("  Command: GET_TIME (0x%02x)\n", CMD_GET_TIME);
    
    // Send request (just the header for GET_TIME)
    if (write(sock_fd, &request.header, sizeof(request.header)) < 0) {
        perror("write() failed");
        close(sock_fd);
        return -1;
    }
    
    // Read response
    ssize_t bytes = read(sock_fd, &response, sizeof(response));
    if (bytes < 0) {
        perror("read() failed");
        close(sock_fd);
        return -1;
    }
    
    if (bytes < (ssize_t)sizeof(response)) {
        printf("Incomplete response received (%zd bytes)\n", bytes);
        close(sock_fd);
        return -1;
    }
    
    // Validate response header
    int validation = validate_header(&response.header);
    if (validation != RESP_OK) {
        if (validation == RESP_BAD_MAGIC) {
            printf("✗ Server sent bad magic number\n");
        } else if (validation == RESP_BAD_VERSION) {
            printf("✗ Server protocol version mismatch\n");
        }
        close(sock_fd);
        return -1;
    }
    
    // Parse response
    if (response.status == RESP_OK) {
        uint32_t timestamp = ntohl(response.timestamp);
        printf("\n✓ Success!\n");
        print_time(timestamp);
    } else {
        printf("✗ Server returned error: 0x%02x\n", response.status);
    }
    
    close(sock_fd);
    return (response.status == RESP_OK) ? 0 : -1;
}

int main(int argc, char *argv[]) {
    char *server_ip = "127.0.0.1";
    
    if (argc > 1) {
        server_ip = argv[1];
    }
    
    printf("Bell Labs Time Client v0.3\n");
    printf("==========================\n\n");
    
    return get_time(server_ip);
}
