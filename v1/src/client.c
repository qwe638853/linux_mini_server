#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 9734
#define BUFFER_SIZE 2048

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in address;

    // 1️⃣ 建立 socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(PORT);

    // 2️⃣ 連線
    if (connect(sockfd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("connect");
        close(sockfd);
        exit(1);
    }

    printf("Connected to server 127.0.0.1:%d\n", PORT);

    // 3️⃣ 將 socket 轉換成 FILE* 以便使用 fprintf/fgets
    FILE *server_fp = fdopen(sockfd, "r+");
    if (server_fp == NULL) {
        perror("fdopen");
        close(sockfd);
        exit(1);
    }

    // 4️⃣ 根據命令行參數決定要發送什麼
    if (argc > 1 && strcmp(argv[1], "SENDMAIL") == 0) {
        // 發送郵件模式
        const char *to = (argc > 2) ? argv[2] : "qwe638853@gmail.com";
        const char *subject = (argc > 3) ? argv[3] : "Test Subject";
        const char *body = (argc > 4) ? argv[4] : "Hello from socket client";

        // 使用 fprintf 發送資料，每行結尾加 \n 以便伺服器的 fgets 讀取
        fprintf(server_fp, "SENDMAIL\n");
        fprintf(server_fp, "%s\n", to);
        fprintf(server_fp, "%s\n", subject);
        fprintf(server_fp, "%s\n", body);
        fflush(server_fp);  // 確保資料立即發送

        printf("Sent mail request:\n");
        printf("  To: %s\n", to);
        printf("  Subject: %s\n", subject);
        printf("  Body: %s\n", body);

    } else {
        // 預設模式：獲取系統資訊（發送其他指令或空行）
        if (argc > 1) {
            // 發送自訂指令
            fprintf(server_fp, "%s\n", argv[1]);
        } else {
            // 不發送任何指令，讓伺服器使用預設行為
            fprintf(server_fp, "SYSINFO\n");
        }
        fflush(server_fp);
    }

    // 5️⃣ 接收伺服器回覆
    printf("\nServer reply:\n");
    printf("----------------------------------------\n");
    
    char buffer[BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), server_fp) != NULL) {
        printf("%s", buffer);
    }
    
    printf("----------------------------------------\n");

    // 6️⃣ 關閉連線
    fclose(server_fp);
    // sockfd 已經被 fclose 關閉了，不需要再 close(sockfd)
    
    return 0;
}
