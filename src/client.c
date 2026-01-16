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
    request_t request;
    response_t response;
    
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
    request.command = CMD_GET_TIME;
    
    // Send request
    if (write(sock_fd, &request, sizeof(request)) < 0) {
        perror("write() failed");
        close(sock_fd);
        return -1;
    }
    
    printf("Sent GET_TIME request\n");
    
    // Read response
    ssize_t bytes = read(sock_fd, &response, sizeof(response));
    if (bytes < 0) {
        perror("read() failed");
        close(sock_fd);
        return -1;
    }
    
    if (bytes < (ssize_t)sizeof(response)) {
        printf("Incomplete response received\n");
        close(sock_fd);
        return -1;
    }
    
    // Parse response
    if (response.status == RESP_OK) {
        uint32_t timestamp = ntohl(response.timestamp);  // Convert from network byte order
        print_time(timestamp);
    } else {
        printf("Server returned error: 0x%02x\n", response.status);
    }
    
    close(sock_fd);
    return 0;
}

int main(int argc, char *argv[]) {
    char *server_ip = "127.0.0.1";
    
    if (argc > 1) {
        server_ip = argv[1];
    }
    
    printf("Bell Labs Time Client v0.2\n");
    printf("==========================\n\n");
    
    return get_time(server_ip);
}
