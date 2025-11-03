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
            
            // redirect stdout and stderr to client socket
            dup2(cfd, STDOUT_FILENO);
            dup2(cfd, STDERR_FILENO);
            close(cfd);
            
            // send system information
            get_hostname();
            get_local_time();
            get_os_info();
            get_memory_usage();
            get_user_info();
            get_disk_info();
            get_env_info();
            get_network_info();
            
            // close connection and exit
            fflush(stdout);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
            exit(0);
        } else {
            // parent: no need to client socket
            close(cfd);
        }
        
    }

}