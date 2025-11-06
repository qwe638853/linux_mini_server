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
#include <sys/time.h>
#include <sys/select.h>
#include "sysinfo.h"
#include "smtp.h"
#include "env.h"
#include "debug.h"


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

// 發送系統資訊的共用函數
static void send_system_info(FILE *client_fp) {
    if(fprintf(client_fp, "System Info:\n") < 0){
        WARN_LOG(stderr, "Failed to write to client\n");
        return;
    }
    if(fflush(client_fp) != 0){
        WARN_LOG(stderr, "fflush() failed\n");
    }

    get_hostname(client_fp);
    get_local_time(client_fp);
    get_os_info(client_fp);
    get_memory_usage(client_fp);
    get_user_info(client_fp);
    get_disk_info(client_fp);
    get_env_info(client_fp);
    get_network_info(client_fp);
}

// 清理資源並退出的共用函數
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
        // 非致命錯誤，繼續執行
    }
    
    memset(&sa_pipe, 0, sizeof(sa_pipe));
    sa_pipe.sa_handler = SIG_IGN;
    if(sigaction(SIGPIPE, &sa_pipe, NULL) < 0){
        WARN_LOG(stderr, "sigaction(SIGPIPE) failed\n");
        perror("sigaction");
        // 非致命錯誤，繼續執行
    }
    
    DEBUG_LOG(stderr, "Signal handlers configured\n");
    
    printf("server listening on 127.0.0.1:9734\n");


    while(1){
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        DEBUG_LOG(stderr, "Waiting for client connection...\n");
        int cfd = accept(server_sockfd, (struct sockaddr *)&cli, &clilen);
        if (cfd < 0) {
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
            // child: 不需要 listening socket
            DEBUG_LOG(stderr, "Child process started (PID: %d)\n", getpid());
            close(server_sockfd);
            
            // 將 socket file descriptor 轉換成 FILE* 以便使用 fgets
            FILE *client_fp = fdopen(cfd, "r+");
            if (client_fp == NULL) {
                ERROR_LOG(stderr, "fdopen() failed\n");
                perror("fdopen");
                close(cfd);
                exit(1);
            }
            DEBUG_LOG(stderr, "Client file stream opened\n");
            
            // 使用 fgets 讀取客戶端資料
            char command[256];
            char to[256];
            char subject[256];
            char body[1024];
            
            /* 使用 select() 檢查 socket 是否可讀（實現超時）
            fd_set readfds;
            struct timeval select_timeout;
            FD_ZERO(&readfds);
            FD_SET(cfd, &readfds);
            select_timeout.tv_sec = 30;
            select_timeout.tv_usec = 0;
            
            int select_result = select(cfd + 1, &readfds, NULL, NULL, &select_timeout);
            if (select_result <= 0) {
                // 超時或錯誤
                if (select_result == 0) {
                    WARN_LOG(stderr, "No command received: timeout waiting for data\n");
                } else {
                    WARN_LOG(stderr, "No command received: select() error\n");
                    perror("select");
                }
                cleanup_and_exit(client_fp, cfd);
            }
                */
            
            // 讀取指令（socket 可讀，可以安全調用 fgets）
            if (fgets(command, sizeof(command), client_fp) != NULL) {
                // 檢查是否因為緩衝區太小而截斷（行太長）
                if(strlen(command) == sizeof(command) - 1 && command[sizeof(command) - 2] != '\n'){
                    WARN_LOG(stderr, "Command line too long, may be truncated\n");
                    // 清空輸入緩衝區，避免後續讀取錯誤
                    int c;
                    while ((c = fgetc(client_fp)) != EOF && c != '\n');
                }
                // 移除換行符
                command[strcspn(command, "\r\n")] = '\0';
                
                // 驗證命令不為空
                if (strlen(command) == 0) {
                    WARN_LOG(stderr, "Empty command received\n");
                    fprintf(client_fp, "Error: Empty command\n");
                    fflush(client_fp);
                    fclose(client_fp);
                    close(cfd);
                    exit(0);
                }
                
                INFO_LOG(stderr, "Received command: %s\n", command);
                
                // 根據指令處理
                if (strcmp(command, "SENDMAIL") == 0) {
                    INFO_LOG(stderr, "Processing SENDMAIL command\n");
                    // 讀取收件人
                    if (fgets(to, sizeof(to), client_fp) != NULL) {
                        to[strcspn(to, "\r\n")] = '\0';
                        DEBUG_LOG(stderr, "To: %s\n", to);
                    }
                    // 讀取主旨
                    if (fgets(subject, sizeof(subject), client_fp) != NULL) {
                        subject[strcspn(subject, "\r\n")] = '\0';
                        DEBUG_LOG(stderr, "Subject: %s\n", subject);
                    }
                    // 讀取內容
                    if (fgets(body, sizeof(body), client_fp) != NULL) {
                        body[strcspn(body, "\r\n")] = '\0';
                        DEBUG_LOG(stderr, "Body length: %zu\n", strlen(body));
                    }
                    
                    // 處理郵件發送（這裡可以呼叫 send_email 函數）
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
                    // 關閉連線
                    cleanup_and_exit(client_fp, cfd);
                } else if (strcmp(command, "SYSINFO") == 0) {
                    // 明確處理 SYSINFO 命令
                    INFO_LOG(stderr, "Processing SYSINFO command\n");
                    send_system_info(client_fp);
                    cleanup_and_exit(client_fp, cfd);
                } else {
                    // 其他未知命令：發送系統資訊作為預設行為
                    WARN_LOG(stderr, "Unknown command: %s, sending system info as default\n", command);
                    send_system_info(client_fp);
                    cleanup_and_exit(client_fp, cfd);
                }
            } else {
                // 如果沒有收到指令（可能是客戶端斷開或超時）
                if (ferror(client_fp)) {
                    WARN_LOG(stderr, "No command received: error reading from client (client may have disconnected)\n");
                } else if (feof(client_fp)) {
                    WARN_LOG(stderr, "No command received: client closed connection before sending command\n");
                } else {
                    WARN_LOG(stderr, "No command received: timeout or empty input\n");
                }
                
                // 不發送系統資訊，直接清理並退出
                cleanup_and_exit(client_fp, cfd);
            }
        } else {
            // parent: no need to client socket
            DEBUG_LOG(stderr, "Parent process: closing client socket, continuing to listen\n");
            close(cfd);
        }
        
    }

}