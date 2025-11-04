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
#include "sysinfo.h"
#include "smtp.h"
#include "env.h"


ssize_t read_line(int fd, char *buf, size_t size) {
    ssize_t n = 0;
    char c;
    while (n < (ssize_t)(size - 1)) {
        ssize_t r = read(fd, &c, 1);
        if (r <= 0) break;
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[n++] = c;
    }
    buf[n] = '\0';
    return n;
}

int main(){
    int server_sockfd;
    int server_len;
    /*  create a socket for the server */
    struct sockaddr_in server_address;

    server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(9734);
    server_len = sizeof(server_address);
    bind(server_sockfd, (struct sockaddr *)&server_address, server_len);

    listen(server_sockfd, 10);

    struct sigaction sa, sa_pipe; 
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP | SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa, NULL);
    
    memset(&sa_pipe, 0, sizeof(sa_pipe));
    sa_pipe.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa_pipe, NULL);

    printf("server listening on 127.0.0.1:9734\n");


    while(1){
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int cfd = accept(server_sockfd, (struct sockaddr *)&cli, &clilen);
        if (cfd < 0) { perror("accept"); continue; }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(cfd);
            continue;
        }
        if (pid == 0) {
            // child: 不需要 listening socket
            close(server_sockfd);
            
            // 將 socket file descriptor 轉換成 FILE* 以便使用 fgets
            FILE *client_fp = fdopen(cfd, "r+");
            if (client_fp == NULL) {
                perror("fdopen");
                close(cfd);
                exit(1);
            }
            
            // 使用 fgets 讀取客戶端資料
            char command[256];
            char to[256];
            char subject[256];
            char body[1024];
            
            // 讀取指令
            if (fgets(command, sizeof(command), client_fp) != NULL) {
                // 移除換行符
                command[strcspn(command, "\r\n")] = '\0';
                
                // 根據指令處理
                if (strcmp(command, "SENDMAIL") == 0) {
                    // 讀取收件人
                    if (fgets(to, sizeof(to), client_fp) != NULL) {
                        to[strcspn(to, "\r\n")] = '\0';
                    }
                    // 讀取主旨
                    if (fgets(subject, sizeof(subject), client_fp) != NULL) {
                        subject[strcspn(subject, "\r\n")] = '\0';
                    }
                    // 讀取內容
                    if (fgets(body, sizeof(body), client_fp) != NULL) {
                        body[strcspn(body, "\r\n")] = '\0';
                    }
                    
                    // 處理郵件發送（這裡可以呼叫 send_email 函數）
                    fprintf(client_fp, "Command: %s\n", command);
                    fprintf(client_fp, "To: %s\n", to);
                    fprintf(client_fp, "Subject: %s\n", subject);
                    fprintf(client_fp, "Body: %s\n", body);
                    
                    if(send_email(to, subject, body) < 0){
                        fprintf(client_fp, "Error: Failed to send email\n");
                    } else {
                        fprintf(client_fp, "Email sent successfully\n");
                    }
                    // 關閉連線
                    fflush(client_fp);
                    fclose(client_fp);
                    close(cfd);
                } else {
                    // 預設行為：發送系統資訊
                    fprintf(client_fp, "System Info:\n");
                    fflush(client_fp);

                    // send system information（使用 fprintf 輸出到 client_fp）
                    get_hostname(client_fp);
                    get_local_time(client_fp);
                    get_os_info(client_fp);
                    get_memory_usage(client_fp);
                    get_user_info(client_fp);
                    get_disk_info(client_fp);
                    get_env_info(client_fp);
                    get_network_info(client_fp);

                    // 關閉連線
                    fflush(client_fp);
                    fclose(client_fp);
                    close(cfd);
                }
            } else {
                // 如果沒有收到指令，預設發送系統資訊
                fprintf(client_fp, "System Info:\n");
                fflush(client_fp);
                
                get_hostname(client_fp);
                get_local_time(client_fp);
                get_os_info(client_fp);
                get_memory_usage(client_fp);
                get_user_info(client_fp);
                get_disk_info(client_fp);
                get_env_info(client_fp);
                get_network_info(client_fp);
                

                // 關閉連線
                fflush(client_fp);
                fclose(client_fp);
                close(cfd);
            }
            
            exit(0);
        } else {
            // parent: no need to client socket
            close(cfd);
        }
        
    }

}