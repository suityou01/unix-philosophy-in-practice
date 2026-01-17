#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "protocol.h"

void send_error_response(int client_fd, uint8_t error_code) {
    response_message_t response;
    
    init_request_header(&response.header, 0, sizeof(response) - sizeof(message_header_t));
    response.status = error_code;
    response.timestamp = 0;
    
    write(client_fd, &response, sizeof(response));
}

void send_response(int client_fd, uint8_t status, uint32_t timestamp) {
    response_message_t response;
    
    init_request_header(&response.header, 0, sizeof(response) - sizeof(message_header_t));
    response.status = status;
    response.timestamp = htonl(timestamp);
    
    write(client_fd, &response, sizeof(response));
}

void handle_get_time(int client_fd) {
    time_t current_time = time(NULL);
    
    printf("  → Sending current time: %u\n", (uint32_t)current_time);
    send_response(client_fd, RESP_OK, (uint32_t)current_time);
}

void handle_set_time(int client_fd, set_time_payload_t *payload) {
    uint32_t new_timestamp = ntohl(payload->new_timestamp);
    struct timeval tv;
    
    printf("  ⚠️  VULNERABILITY: SET_TIME with NO AUTHENTICATION!\n");
    printf("  → Requested time: %u\n", new_timestamp);
    
    // Convert timestamp to human-readable
    time_t t = (time_t)new_timestamp;
    struct tm *tm_info = localtime(&t);
    char time_str[26];
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("  → New time would be: %s\n", time_str);
    
    // Convert to timeval structure
    tv.tv_sec = new_timestamp;
    tv.tv_usec = 0;
    
    // Attempt to set system time
    // NOTE: This requires root privileges or CAP_SYS_TIME capability
    if (settimeofday(&tv, NULL) == 0) {
        printf("  ✓ System time CHANGED to %u\n", new_timestamp);
        printf("  ⚠️  THIS IS A SECURITY VULNERABILITY!\n");
        send_response(client_fd, RESP_OK, new_timestamp);
    } else {
        perror("  ✗ settimeofday() failed");
        printf("  ℹ  (Server needs root privileges or CAP_SYS_TIME)\n");
        send_response(client_fd, RESP_ERROR, 0);
    }
}

void handle_client(int client_fd) {
    message_header_t header;
    ssize_t bytes_read;
    
    // Read message header
    bytes_read = read(client_fd, &header, sizeof(header));
    
    if (bytes_read < 0) {
        perror("  ✗ read() failed");
        close(client_fd);
        return;
    }
    
    if (bytes_read == 0) {
        printf("  Client disconnected\n");
        close(client_fd);
        return;
    }
    
    if (bytes_read < (ssize_t)sizeof(header)) {
        printf("  ✗ Incomplete header (%zd bytes)\n", bytes_read);
        close(client_fd);
        return;
    }
    
    // Validate header
    int validation = validate_header(&header);
    if (validation != RESP_OK) {
        if (validation == RESP_BAD_MAGIC) {
            printf("  ✗ Bad magic: 0x%08x\n", ntohl(header.magic));
        } else if (validation == RESP_BAD_VERSION) {
            printf("  ✗ Bad version: %d\n", header.version);
        }
        send_error_response(client_fd, validation);
        close(client_fd);
        return;
    }
    
    uint16_t msg_length = ntohs(header.length);
    uint8_t command = header.command;
    uint16_t payload_size = msg_length - sizeof(message_header_t);
    
    printf("  Message: cmd=0x%02x, length=%d, payload=%d\n", 
           command, msg_length, payload_size);
    
    // Handle command
    switch (command) {
        case CMD_GET_TIME: {
            if (payload_size != 0) {
                printf("  ✗ GET_TIME should have no payload\n");
                send_error_response(client_fd, RESP_ERROR);
                break;
            }
            handle_get_time(client_fd);
            break;
        }
        
        case CMD_SET_TIME: {
            if (payload_size != sizeof(set_time_payload_t)) {
                printf("  ✗ SET_TIME payload size mismatch (expected %zu, got %d)\n",
                       sizeof(set_time_payload_t), payload_size);
                send_error_response(client_fd, RESP_ERROR);
                break;
            }
            
            // Read SET_TIME payload
            set_time_payload_t payload;
            bytes_read = read(client_fd, &payload, sizeof(payload));
            
            if (bytes_read < (ssize_t)sizeof(payload)) {
                printf("  ✗ Incomplete SET_TIME payload\n");
                send_error_response(client_fd, RESP_ERROR);
                break;
            }
            
            handle_set_time(client_fd, &payload);
            break;
        }
        
        default:
            printf("  ✗ Unknown command: 0x%02x\n", command);
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
    
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  Bell Labs Time Server v0.4 - UNSAFE/VULNERABLE VERSION  ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("⚠️  WARNING: THIS VERSION IS DELIBERATELY INSECURE!\n");
    printf("⚠️  ANY CLIENT CAN CHANGE THE SYSTEM TIME!\n");
    printf("⚠️  FOR EDUCATIONAL PURPOSES ONLY!\n");
    printf("\n");
    printf("Protocol: TIME v%d (Magic: 0x%08x)\n", PROTOCOL_VERSION, PROTOCOL_MAGIC);
    printf("\n");
    printf("Supported commands:\n");
    printf("  0x01 - GET_TIME: Query system time\n");
    printf("  0x02 - SET_TIME: Change system time (NO AUTH!)\n");
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
    printf("NOTE: Run with 'sudo ./server' to allow SET_TIME to work\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Accept loop
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept() failed");
            continue;
        }
        
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
        printf("Client: %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        handle_client(client_fd);
        
        printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");
    }
    
    close(server_fd);
    return 0;
}

