#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/select.h>
#include "sysinfo.h"
#include "smtp.h"
#include "env.h"
#include "debug.h"

// Global variable: flag to mark if server should exit
static volatile sig_atomic_t server_should_exit = 0;

// SIGQUIT handler: set exit flag
static void sigquit_handler(int sig) {
    (void)sig; // Avoid unused parameter warning
    server_should_exit = 1;
    INFO_LOG(stderr, "Received SIGQUIT, server will exit gracefully\n");
}

ssize_t read_line(int fd, char *buf, size_t size) {
    DEBUG_LOG(stderr, "read_line: reading from fd %d, buffer size %zu\n", fd, size);
    ssize_t n = 0;
    char c;
    while (n < (ssize_t)(size - 1)) {
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) {
            DEBUG_LOG(stderr, "read_line: read returned %zd\n", r);
            break;
        }
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[n++] = c;
    }
    buf[n] = '\0';
    DEBUG_LOG(stderr, "read_line: read %zd bytes\n", n);
    return n;
}

// Shared function to send system information
static void send_system_info(FILE *client_fp) {
    if(fprintf(client_fp, "System Info:\n") < 0){
        WARN_LOG(stderr, "Failed to write to client\n");
        return;
    }
    if(fflush(client_fp) != 0){
        WARN_LOG(stderr, "fflush() failed\n");
    }
    sleep(10);
    get_hostname(client_fp);
    get_local_time(client_fp);
    get_os_info(client_fp);
    get_memory_usage(client_fp);
    get_user_info(client_fp);
    get_disk_info(client_fp);
    get_env_info(client_fp);
    get_network_info(client_fp);
}

// Shared function to cleanup resources and exit
static void cleanup_and_exit(FILE *client_fp, int cfd) {
    if(fflush(client_fp) != 0){
        WARN_LOG(stderr, "fflush() failed\n");
    }
    if(fclose(client_fp) != 0){
        WARN_LOG(stderr, "fclose() failed\n");
    }
    close(cfd);
    DEBUG_LOG(stderr, "Child process exiting\n");
    exit(0);
}

int main(int argc, char *argv[]){
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
    }
    
    INFO_LOG(stderr, "Server starting...\n");
    int server_sockfd;
    int server_len;
    /*  create a socket for the server */
    struct sockaddr_in server_address;

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        ERROR_LOG(stderr, "socket() failed\n");
        perror("socket");
        return 1;
    }
    DEBUG_LOG(stderr, "Socket created: fd %d\n", server_sockfd);
    
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    if(server_address.sin_addr.s_addr == INADDR_NONE){
        ERROR_LOG(stderr, "inet_addr() failed: invalid address\n");
        close(server_sockfd);
        return 1;
    }
    server_address.sin_port = htons(9734);
    server_len = sizeof(server_address);
    
    if (bind(server_sockfd, (struct sockaddr *)&server_address, server_len) < 0) {
        ERROR_LOG(stderr, "bind() failed\n");
        perror("bind");
        close(server_sockfd);
        return 1;
    }
    INFO_LOG(stderr, "Socket bound to 127.0.0.1:9734\n");

    if (listen(server_sockfd, 10) < 0) {
        ERROR_LOG(stderr, "listen() failed\n");
        perror("listen");
        close(server_sockfd);
        return 1;
    }
    DEBUG_LOG(stderr, "Listening with backlog 10\n");

    
    struct sigaction sa; 
    struct sigaction  sa_pipe; 
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
    if(sigaction(SIGCHLD, &sa, NULL) < 0){
        WARN_LOG(stderr, "sigaction(SIGCHLD) failed\n");
        perror("sigaction");
        // Non-fatal error, continue execution
    }
    
    
    memset(&sa_pipe, 0, sizeof(sa_pipe));
    sa_pipe.sa_handler = SIG_IGN;
    if(sigaction(SIGPIPE, &sa_pipe, NULL) < 0){
        WARN_LOG(stderr, "sigaction(SIGPIPE) failed\n");
        perror("sigaction");
        // Non-fatal error, continue execution
    }
    
    // Set SIGINT (Ctrl+C) to ignore, don't exit server
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = SIG_IGN;
    if(sigaction(SIGINT, &sa_int, NULL) < 0){
        WARN_LOG(stderr, "sigaction(SIGINT) failed\n");
        perror("sigaction");
        // Non-fatal error, continue execution
    }
    
    // Set SIGQUIT (Ctrl+\) handler for graceful shutdown
    // Note: don't use SA_RESTART so accept() can be interrupted
    struct sigaction sa_quit;
    memset(&sa_quit, 0, sizeof(sa_quit));
    sa_quit.sa_handler = sigquit_handler;
    sa_quit.sa_flags = 0; // Don't use SA_RESTART, allow accept() to be interrupted
    if(sigaction(SIGQUIT, &sa_quit, NULL) < 0){
        WARN_LOG(stderr, "sigaction(SIGQUIT) failed\n");
        perror("sigaction");
        // Non-fatal error, continue execution
    }
    
    DEBUG_LOG(stderr, "Signal handlers configured\n");
    INFO_LOG(stderr, "Press Ctrl+/ (SIGQUIT) to exit server, Ctrl+C (SIGINT) is ignored\n");
    
    printf("server listening on 127.0.0.1:9734\n");
    printf("Press Ctrl+/ to exit server (Ctrl+C is ignored)\n");


    while(!server_should_exit){
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        DEBUG_LOG(stderr, "Waiting for client connection...\n");
        int cfd = accept(server_sockfd, (struct sockaddr *)&cli, &clilen);
        if (cfd < 0) {
            // Check if interrupted by SIGQUIT
            if (errno == EINTR && server_should_exit) {
                DEBUG_LOG(stderr, "accept() interrupted by SIGQUIT, exiting...\n");
                break;
            }
            // If interrupted by other signal but not exit signal, continue waiting
            if (errno == EINTR) {
                continue;
            }
            ERROR_LOG(stderr, "accept() failed\n");
            perror("accept");
            continue;
        }
        INFO_LOG(stderr, "Client connected from %s:%d\n", inet_ntoa(cli.sin_addr), ntohs(cli.sin_port));

        pid_t pid = fork();
        if (pid < 0) {
            ERROR_LOG(stderr, "fork() failed\n");
            perror("fork");
            close(cfd);
            continue;
        }
        if (pid == 0) {
            // child: don't need listening socket
            DEBUG_LOG(stderr, "Child process started (PID: %d)\n", getpid());
            close(server_sockfd);
            
            // Convert socket file descriptor to FILE* for fgets usage
            FILE *client_fp = fdopen(cfd, "r+");
            if (client_fp == NULL) {
                ERROR_LOG(stderr, "fdopen() failed\n");
                perror("fdopen");
                close(cfd);
                exit(1);
            }
            DEBUG_LOG(stderr, "Client file stream opened\n");
            
            // Use fgets to read client data
            char command[256];
            char to[256];
            char subject[256];
            char body[1024];
            
            // Use select() to check if socket is readable (implement timeout)
            fd_set readfds;
            struct timeval select_timeout;
            FD_ZERO(&readfds);
            FD_SET(cfd, &readfds);
            select_timeout.tv_sec = 30;
            select_timeout.tv_usec = 0;
            
            int select_result = select(cfd + 1, &readfds, NULL, NULL, &select_timeout);
            if (select_result <= 0) {
                // Timeout or error
                if (select_result == 0) {
                    WARN_LOG(stderr, "No command received: timeout waiting for data\n");
                } else {
                    WARN_LOG(stderr, "No command received: select() error\n");
                    perror("select");
                }
                cleanup_and_exit(client_fp, cfd);
            }
        
            // Limit data read to prevent large data stream attacks
            // Use read() directly, read at most 256 bytes (command buffer size)
            char temp_buf[256];
            ssize_t bytes_read = read(cfd, temp_buf, sizeof(temp_buf) - 1);
            if (bytes_read <= 0) {
                WARN_LOG(stderr, "Failed to read command or connection closed\n");
                cleanup_and_exit(client_fp, cfd);
            }
            temp_buf[bytes_read] = '\0';
            
            // Check if newline character exists
            char *newline = strchr(temp_buf, '\n');
            
            if (newline == NULL && bytes_read == sizeof(temp_buf) - 1) {
                // No newline and buffer full, likely a large data stream attack
                WARN_LOG(stderr, "Large data stream detected without newline, closing connection\n");
                cleanup_and_exit(client_fp, cfd);
            }
            
            // Copy data to command buffer
            if (newline != NULL) {
                *newline = '\0';
            }
            // Remove carriage return
            char *carriage = strchr(temp_buf, '\r');
            if (carriage != NULL) {
                *carriage = '\0';
            }
            strncpy(command, temp_buf, sizeof(command) - 1);
            command[sizeof(command) - 1] = '\0';
            
            // Read command (socket is readable, safe to call fgets)
            if (strlen(command) > 0) {
                INFO_LOG(stderr, "Received command: %s\n", command);
                
                // Process according to command
                if (strcmp(command, "SENDMAIL") == 0) {
                    INFO_LOG(stderr, "Processing SENDMAIL command\n");
                    // Read recipient (use read_line to limit read amount)
                    if (read_line(cfd, to, sizeof(to)) > 0) {
                        DEBUG_LOG(stderr, "To: %s\n", to);
                    }
                    // Read subject
                    if (read_line(cfd, subject, sizeof(subject)) > 0) {
                        DEBUG_LOG(stderr, "Subject: %s\n", subject);
                    }
                    // Read body
                    if (read_line(cfd, body, sizeof(body)) > 0) {
                        DEBUG_LOG(stderr, "Body length: %zu\n", strlen(body));
                    }
                    
                    // Process email sending (can call send_email function here)
                    if(fprintf(client_fp, "Command: %s\n", command) < 0 ||
                       fprintf(client_fp, "To: %s\n", to) < 0 ||
                       fprintf(client_fp, "Subject: %s\n", subject) < 0 ||
                       fprintf(client_fp, "Body: %s\n", body) < 0){
                        WARN_LOG(stderr, "Failed to write response to client\n");
                    }
                    
                    INFO_LOG(stderr, "Sending email to %s\n", to);
                    if(send_email(to, subject, body) < 0){
                        ERROR_LOG(stderr, "Failed to send email to %s\n", to);
                        fprintf(client_fp, "Error: Failed to send email\n");
                    } else {
                        INFO_LOG(stderr, "Email sent successfully to %s\n", to);
                        fprintf(client_fp, "Email sent successfully\n");
                    }
                    // Close connection
                    cleanup_and_exit(client_fp, cfd);
                } else if (strcmp(command, "SYSINFO") == 0) {
                    // Explicitly handle SYSINFO command
                    INFO_LOG(stderr, "Processing SYSINFO command\n");
                    send_system_info(client_fp);
                    cleanup_and_exit(client_fp, cfd);
                } else {
                    // Other unknown commands
                    WARN_LOG(stderr, "Unknown command: %s\n", command);
                    cleanup_and_exit(client_fp, cfd);
                }
            } else {
                // If no command received (client may have disconnected or timeout)
                WARN_LOG(stderr, "No command received: empty command or connection issue\n");
                cleanup_and_exit(client_fp, cfd);
            }
        } else {
            // parent: no need to client socket
            DEBUG_LOG(stderr, "Parent process: closing client socket, continuing to listen\n");
            close(cfd);
        }
        
    }
    
    // Graceful exit: close listening socket
    INFO_LOG(stderr, "Server shutting down gracefully...\n");
    if(close(server_sockfd) < 0){
        WARN_LOG(stderr, "close(server_sockfd) failed\n");
        perror("close");
    }
    INFO_LOG(stderr, "Server exited\n");
    return 0;

}