#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in server_address;
    char buffer[4096];
    int n;

    // 解析命令行參數（可選的伺服器地址和端口）
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    int port = (argc > 2) ? atoi(argv[2]) : 9734;

    // 創建 socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // 設置伺服器地址
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_aton(server_ip, &server_address.sin_addr) == 0) {
        fprintf(stderr, "Invalid IP address: %s\n", server_ip);
        close(sockfd);
        return 1;
    }

    // 連接到伺服器
    printf("Connecting to %s:%d...\n", server_ip, port);
    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected! Receiving data...\n\n");

    // 接收並打印數據
    while ((n = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        printf("%s", buffer);
        fflush(stdout);
    }

    if (n < 0) {
        perror("read");
    }

    printf("\nConnection closed.\n");
    close(sockfd);
    return 0;
}

