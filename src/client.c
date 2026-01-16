#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

int main(int argc, char *argv[]) {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char *server_ip = "127.0.0.1";
    
    if (argc > 1) {
        server_ip = argv[1];
    }
    
    // Create socket
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket() failed");
        exit(1);
    }
    
    // Connect to server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TIMESERVER_PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() failed");
        close(sock_fd);
        exit(1);
    }
    
    printf("Connected to %s:%d\n", server_ip, TIMESERVER_PORT);
    
    // Send test message
    const char *msg = "Hello, timeserver!";
    write(sock_fd, msg, strlen(msg));
    
    // Read response
    ssize_t bytes = read(sock_fd, buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("Server response: %s\n", buffer);
    }
    
    close(sock_fd);
    return 0;
}
