#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    // For now, just echo back what we receive
    bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("Received: %s\n", buffer);
        
        // Echo back
        write(client_fd, buffer, bytes_read);
    }
    
    close(client_fd);
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    printf("Bell Labs Style Time Server v0.1\n");
    printf("=================================\n\n");
    
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
