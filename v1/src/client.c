#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "debug.h"

#define PORT 9734
#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    // Runtime debug log control: check environment variable
    const char *debug_env = getenv("DEBUG_LOG");
    if (debug_env != NULL && strcmp(debug_env, "1") == 0) {
        debug_log_enable();
        const char *level_env = getenv("DEBUG_LOG_LEVEL");
        if (level_env != NULL) {
            int level = atoi(level_env);
            if (level >= LOG_ERROR && level <= LOG_DEBUG) {
                debug_log_set_level((log_level_t)level);
            }
        }
    }
    
    // Runtime debug log control: check command line arguments
    // Note: We process debug arguments but don't modify argv to keep it simple
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
            debug_log_enable();
            if (i + 1 < argc) {
                int level = atoi(argv[i + 1]);
                if (level >= LOG_ERROR && level <= LOG_DEBUG) {
                    debug_log_set_level((log_level_t)level);
                    i++; // skip next argument
                }
            }
        } else if (strcmp(argv[i], "--debug-disable") == 0) {
            debug_log_disable();
        }
        // Other arguments are processed normally below
    }
    
    INFO_LOG(stderr, "Client starting...\n");
    int sockfd;
    struct sockaddr_in address;

    // Step 1: Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ERROR_LOG(stderr, "socket() failed\n");
        perror("socket");
        exit(1);
    }
    DEBUG_LOG(stderr, "Socket created: fd %d\n", sockfd);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(address.sin_addr.s_addr == INADDR_NONE){
        ERROR_LOG(stderr, "inet_addr() failed: invalid address\n");
        close(sockfd);
        exit(1);
    }
    address.sin_port = htons(PORT);

    // Set connection timeout (10 seconds)
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        WARN_LOG(stderr, "setsockopt(SO_SNDTIMEO) failed\n");
        perror("setsockopt");
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        WARN_LOG(stderr, "setsockopt(SO_RCVTIMEO) failed\n");
        perror("setsockopt");
    }

    // Step 2: Connect
    INFO_LOG(stderr, "Connecting to server 127.0.0.1:%d...\n", PORT);
    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        ERROR_LOG(stderr, "connect() failed\n");
        perror("connect");
        close(sockfd);
        exit(1);
    }
    INFO_LOG(stderr, "Connected to server\n");

    printf("Connected to server 127.0.0.1:%d\n", PORT);

    // Step 3: Convert socket to FILE* for fprintf/fgets usage
    FILE *server_fp = fdopen(sockfd, "r+");
    if (server_fp == NULL) {
        ERROR_LOG(stderr, "fdopen() failed\n");
        perror("fdopen");
        close(sockfd);
        exit(1);
    }
    DEBUG_LOG(stderr, "Server file stream opened\n");

    // Step 4: Determine what to send based on command line arguments (skip debug arguments)
    // Find first non-debug argument
    int cmd_idx = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0 ||
            strcmp(argv[i], "--debug-disable") == 0) {
            i++; // Skip this and possibly next (if level was specified)
            if (i < argc && strcmp(argv[i-1], "--debug") == 0) {
                // Check if next is a number (level)
                char *end;
                strtol(argv[i], &end, 10);
                if (*end == '\0') i++; // Skip level number
            }
            continue;
        }
        cmd_idx = i;
        break;
    }
    
    if (cmd_idx < argc && strcmp(argv[cmd_idx], "SENDMAIL") == 0) {
        // Send email mode
        INFO_LOG(stderr, "Sending SENDMAIL command\n");
        const char *to = (cmd_idx + 1 < argc) ? argv[cmd_idx + 1] : "qwe638853@gmail.com";
        const char *subject = (cmd_idx + 2 < argc) ? argv[cmd_idx + 2] : "Test Subject";
        const char *body = (cmd_idx + 3 < argc) ? argv[cmd_idx + 3] : "Hello from socket client";

        DEBUG_LOG(stderr, "Email details - To: %s, Subject: %s\n", to, subject);

        // Send all parameters in one line, separated by | (pipe character)
        // Format: SENDMAIL|to|subject|body
        if(fprintf(server_fp, "SENDMAIL|%s|%s|%s\n", to, subject, body) < 0){
            ERROR_LOG(stderr, "Failed to send data to server\n");
            fclose(server_fp);
            exit(1);
        }
        if(fflush(server_fp) != 0){
            ERROR_LOG(stderr, "fflush() failed\n");
            fclose(server_fp);
            exit(1);
        }
        DEBUG_LOG(stderr, "SENDMAIL command sent\n");

        printf("Sent mail request:\n");
        printf("  To: %s\n", to);
        printf("  Subject: %s\n", subject);
        printf("  Body: %s\n", body);

    } else {
        // Default mode: get system information (send other command or empty line)
        if (cmd_idx < argc) {
            // Send custom command
            INFO_LOG(stderr, "Sending custom command: %s\n", argv[cmd_idx]);
            if(fprintf(server_fp, "%s\n", argv[cmd_idx]) < 0){
                ERROR_LOG(stderr, "Failed to send command to server\n");
                fclose(server_fp);
                exit(1);
            }
        } else {
            // Send SYSINFO command (different from nc: nc sends nothing triggers server timeout)
            // Explicitly sending SYSINFO here gets immediate response without waiting for timeout
            INFO_LOG(stderr, "Sending SYSINFO command\n");
            if(fprintf(server_fp, "SYSINFO\n") < 0){
                ERROR_LOG(stderr, "Failed to send command to server\n");
                fclose(server_fp);
                exit(1);
            }
        }
        if(fflush(server_fp) != 0){
            ERROR_LOG(stderr, "fflush() failed\n");
            fclose(server_fp);
            exit(1);
        }
    }

    // Step 5: Receive server reply
    INFO_LOG(stderr, "Waiting for server reply...\n");
    printf("\nServer reply:\n");
    printf("----------------------------------------\n");
    
    char buffer[BUFFER_SIZE];
    int line_count = 0;
    while (fgets(buffer, sizeof(buffer), server_fp) != NULL) {
        printf("%s", buffer);
        line_count++;
    }
    // Check if ended due to error
    if(ferror(server_fp)){
        ERROR_LOG(stderr, "Error reading from server\n");
    }
    DEBUG_LOG(stderr, "Received %d lines from server\n", line_count);
    
    printf("----------------------------------------\n");

    // Step 6: Close connection
    INFO_LOG(stderr, "Closing connection\n");
    if(fclose(server_fp) != 0){
        WARN_LOG(stderr, "fclose() failed\n");
    }
    // sockfd is already closed by fclose(), no need to close(sockfd) again
    
    return 0;
}
