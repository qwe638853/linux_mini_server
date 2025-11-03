#define _GNU_SOURCE
#include "../include/sysinfo.h"

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>  // for ioctl
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>    // for IFNAMSIZ
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>    // for SIOCGIFFLAGS, SIOCGIFMTU, SIOCGIFNETMASK, SIOCGIFBRDADDR
#include <ifaddrs.h> // for getifaddrs


int get_hostname(){
    char hostname[256] = {0};
    gethostname(hostname, sizeof(hostname));
    printf("Hostname: %s\n", hostname);
    return 0;
}

// single thread
int get_local_time(){
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    printf("Local Time: %s\n", asctime(local_time));
    return 0;
}

int get_os_info(){
    struct utsname name;
    if(uname(&name) < 0){
        perror("uname");
        return -1;
    }
    printf("OS: %s\n", name.sysname);
    printf("Release: %s\n", name.release);
    printf("Version: %s\n", name.version);
    printf("Machine: %s\n", name.machine);
    return 0;
}

int get_memory_usage(){
    struct sysinfo info;
    if(sysinfo(&info) < 0){
        perror("sysinfo");
        return -1;
    }
    printf("Uptime:      %ld seconds\n", info.uptime);
    printf("Load Avg:    %.2f %.2f %.2f (1/5/15 min)\n", 
               info.loads[0] / 65536.0, 
               info.loads[1] / 65536.0, 
               info.loads[2] / 65536.0);
    printf("Total RAM:   %ld bytes\n", info.totalram);
    printf("Free RAM:    %ld bytes\n", info.freeram);
    printf("Used RAM:    %ld bytes\n", info.totalram - info.freeram);
    printf("Memory Usage: %f%%\n", (float)(info.totalram - info.freeram) / info.totalram * 100);
    return 0;
}

int get_user_info(){
    struct passwd *pw = getpwuid(getuid());
    if(pw == NULL){
        perror("getpwuid");
        return -1;
    }
    printf("User: %s\n", pw->pw_name);
    printf("Home: %s\n", pw->pw_dir);
    return 0;
}

int get_disk_info(){
    struct statvfs info;
    if(statvfs("/", &info) < 0){
        perror("statvfs");
        return -1;
    }
    unsigned long long total_bytes = (unsigned long long)info.f_blocks * info.f_frsize;
    unsigned long long free_bytes  = (unsigned long long)info.f_bfree  * info.f_frsize;
    unsigned long long used_bytes  = total_bytes - free_bytes;

    double usage = (double)used_bytes / (double)total_bytes * 100.0;

    printf("Disk Usage : %.2f%%\n", usage);
    printf("Total Space: %llu bytes\n", total_bytes);
    printf("Free Space : %llu bytes\n", free_bytes);
    printf("Used Space : %llu bytes\n", used_bytes);
    return 0;
}

extern char **environ;

int get_env_info(){
    printf("=== Environment Variables ===\n");

    for (char **env = environ; *env != NULL; env++) {
        printf("%s\n", *env);
    }

    return 0;
}

int get_network_info(){
    // initialize ifaddrs
    struct ifaddrs *ifaddr = NULL, *ifa = NULL;
    if(getifaddrs(&ifaddr) < 0){
        perror("getifaddrs");
        return -1;
    }
    printf("=== Network Interfaces ===\n");

    // iterate through all interfaces
    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next){
        if(!ifa->ifa_addr){
            continue;
        }
        
        // 先打印介面名稱和 IPv4 地址（如果有的話）
        if(ifa->ifa_addr->sa_family == AF_INET){
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            printf("%s: %s\n", ifa->ifa_name, inet_ntoa(sa->sin_addr));
        } else {
            // 非 IPv4 地址也顯示介面名稱
            printf("%s:\n", ifa->ifa_name);
        }

        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ-1);
        
        // 獲取 MAC 地址
        if(ioctl(fd, SIOCGIFHWADDR, &ifr) == 0){
            unsigned char *mac_addr = (unsigned char *)ifr.ifr_hwaddr.sa_data;
            // 檢查是否全為 0
            int is_zero = 1;
            for(int i = 0; i < 6; i++){
                if(mac_addr[i] != 0){
                    is_zero = 0;
                    break;
                }
            }
            if(is_zero){
                printf("  MAC Address: NA\n");
            } else {
                printf("  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            }
        }
        
        if(ioctl(fd, SIOCGIFMTU, &ifr) == 0){
            printf("  MTU: %d\n", ifr.ifr_mtu);
        }
        // 只為 IPv4 顯示 Netmask 和 Broadcast
        if(ifa->ifa_addr->sa_family == AF_INET){
            if(ioctl(fd, SIOCGIFNETMASK, &ifr) == 0){
                struct sockaddr_in *mask = (struct sockaddr_in *)&ifr.ifr_netmask;
                printf("  Netmask: %s\n", inet_ntoa(mask->sin_addr));
            }
            if(ioctl(fd, SIOCGIFBRDADDR, &ifr) == 0){
                struct sockaddr_in *brd = (struct sockaddr_in *)&ifr.ifr_broadaddr;
                printf("  Broadcast: %s\n", inet_ntoa(brd->sin_addr));
            }
        }
        close(fd);
    }
    
    freeifaddrs(ifaddr);
    return 0;
}



