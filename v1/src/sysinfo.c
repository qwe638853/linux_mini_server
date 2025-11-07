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

int get_hostname(FILE *fp){
    INFO_LOG(fp, "Getting hostname...\n");
    fprintf(fp, "=== Hostname ===\n");
    char hostname[256] = {0};
    if(gethostname(hostname, sizeof(hostname)) < 0){
        ERROR_LOG(fp, "gethostname() failed\n");
        perror("gethostname");
        return -1;
    }
    DEBUG_LOG(fp, "Hostname retrieved: %s\n", hostname);
    fprintf(fp, "Hostname: %s\n", hostname);
    return 0;
}

// single thread
int get_local_time(FILE *fp){
    INFO_LOG(fp, "Getting local time...\n");
    fprintf(fp, "=== Local Time ===\n");
    time_t now = time(NULL);
    if(now == (time_t)-1){
        ERROR_LOG(fp, "time() failed\n");
        perror("time");
        return -1;
    }
    struct tm *local_time = localtime(&now);
    if(local_time == NULL){
        ERROR_LOG(fp, "localtime() failed\n");
        perror("localtime");
        return -1;
    }
    DEBUG_LOG(fp, "Time retrieved: %ld\n", now);
    fprintf(fp, "Local Time: %s\n", asctime(local_time));
    return 0;
}

int get_os_info(FILE *fp){
    INFO_LOG(fp, "Getting OS information...\n");
    fprintf(fp, "=== Operating System ===\n");
    struct utsname name;
    if(uname(&name) < 0){
        ERROR_LOG(fp, "uname() failed\n");
        perror("uname");
        return -1;
    }
    INFO_LOG(fp, "OS: %s, Release: %s, Machine: %s\n", name.sysname, name.release, name.machine);
    fprintf(fp, "OS: %s\n", name.sysname);
    fprintf(fp, "Release: %s\n", name.release);
    fprintf(fp, "Version: %s\n", name.version);
    fprintf(fp, "Machine: %s\n", name.machine);
    return 0;
}

int get_memory_usage(FILE *fp){
    INFO_LOG(fp, "Getting memory usage...\n");
    fprintf(fp, "=== System Resources ===\n");
    struct sysinfo info;
    if(sysinfo(&info) < 0){
        ERROR_LOG(fp, "sysinfo() failed\n");
        perror("sysinfo");
        return -1;
    }
    // Check for division by zero
    if(info.totalram == 0){
        ERROR_LOG(fp, "sysinfo() returned zero total RAM\n");
        fprintf(fp, "Error: Invalid system information (zero total RAM)\n");
        return -1;
    }
    float mem_usage = (float)(info.totalram - info.freeram) / info.totalram * 100;
    INFO_LOG(fp, "Memory usage: %.2f%%, Uptime: %ld seconds\n", mem_usage, info.uptime);
    fprintf(fp, "Uptime:      %ld seconds\n", info.uptime);
    fprintf(fp, "Load Avg:    %.2f %.2f %.2f (1/5/15 min)\n", 
               info.loads[0] / 65536.0, 
               info.loads[1] / 65536.0, 
               info.loads[2] / 65536.0);
    fprintf(fp, "Total RAM:   %ld bytes\n", info.totalram);
    fprintf(fp, "Free RAM:    %ld bytes\n", info.freeram);
    fprintf(fp, "Used RAM:    %ld bytes\n", info.totalram - info.freeram);
    fprintf(fp, "Memory Usage: %f%%\n", mem_usage);
    return 0;
}

int get_user_info(FILE *fp){
    INFO_LOG(fp, "Getting user information...\n");
    fprintf(fp, "=== User Information ===\n");
    uid_t uid = getuid();
    DEBUG_LOG(fp, "Current UID: %d\n", uid);
    struct passwd *pw = getpwuid(uid);
    if(pw == NULL){
        ERROR_LOG(fp, "getpwuid() failed for UID: %d\n", uid);
        perror("getpwuid");
        return -1;
    }
    INFO_LOG(fp, "User: %s, Home: %s\n", pw->pw_name, pw->pw_dir);
    fprintf(fp, "User: %s\n", pw->pw_name);
    fprintf(fp, "Home: %s\n", pw->pw_dir);
    return 0;
}

int get_disk_info(FILE *fp){
    INFO_LOG(fp, "Getting disk information for root filesystem...\n");
    fprintf(fp, "=== Disk Usage ===\n");
    struct statvfs info;
    if(statvfs("/", &info) < 0){
        ERROR_LOG(fp, "statvfs() failed for /\n");
        perror("statvfs");
        return -1;
    }
    unsigned long long total_bytes = (unsigned long long)info.f_blocks * info.f_frsize;
    unsigned long long free_bytes  = (unsigned long long)info.f_bfree  * info.f_frsize;
    unsigned long long used_bytes  = total_bytes - free_bytes;

    // Check for division by zero
    if(total_bytes == 0){
        ERROR_LOG(fp, "statvfs() returned zero total disk space\n");
        fprintf(fp, "Error: Invalid filesystem information (zero total space)\n");
        return -1;
    }

    double usage = (double)used_bytes / (double)total_bytes * 100.0;
    INFO_LOG(fp, "Disk usage: %.2f%%, Total: %llu bytes, Free: %llu bytes\n", 
              usage, total_bytes, free_bytes);

    fprintf(fp, "Disk Usage : %.2f%%\n", usage);
    fprintf(fp, "Total Space: %llu bytes\n", total_bytes);
    fprintf(fp, "Free Space : %llu bytes\n", free_bytes);
    fprintf(fp, "Used Space : %llu bytes\n", used_bytes);
    return 0;
}

extern char **environ;

int get_env_info(FILE *fp){
    INFO_LOG(fp, "Getting environment variables...\n");
    int count = 0;
    for (char **env = environ; *env != NULL; env++) {
        count++;
    }
    INFO_LOG(fp, "Found %d environment variables\n", count);
    fprintf(fp, "=== Environment Variables ===\n");

    for (char **env = environ; *env != NULL; env++) {
        fprintf(fp, "%s\n", *env);
    }

    return 0;
}

int get_network_info(FILE *fp){
    INFO_LOG(fp, "Getting network interface information...\n");
    // initialize ifaddrs
    struct ifaddrs *ifaddr = NULL, *ifa = NULL;
    if(getifaddrs(&ifaddr) < 0){
        ERROR_LOG(fp, "getifaddrs() failed\n");
        perror("getifaddrs");
        return -1;
    }
    DEBUG_LOG(fp, "getifaddrs() succeeded\n");
    fprintf(fp, "=== Network Interfaces ===\n");

    int interface_count = 0;
    // iterate through all interfaces
    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next){
        if(!ifa->ifa_addr){
            DEBUG_LOG(fp, "Skipping interface %s (no address)\n", ifa->ifa_name ? ifa->ifa_name : "unknown");
            continue;
        }
        
        interface_count++;
        DEBUG_LOG(fp, "Processing interface: %s (family: %d)\n", ifa->ifa_name, ifa->ifa_addr->sa_family);
        
        // Print interface name and IPv4 address first (if available)
        if(ifa->ifa_addr->sa_family == AF_INET){
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            DEBUG_LOG(fp, "  IPv4 address: %s\n", inet_ntoa(sa->sin_addr));
            fprintf(fp, "%s: %s\n", ifa->ifa_name, inet_ntoa(sa->sin_addr));
        } else {
            // Also show interface name for non-IPv4 addresses
            DEBUG_LOG(fp, "  Non-IPv4 interface\n");
            fprintf(fp, "%s:\n", ifa->ifa_name);
        }

        int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(fd < 0){
            WARN_LOG(fp, "  Failed to create socket for interface %s\n", ifa->ifa_name);
            continue;
        }
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, ifa->ifa_name, IFNAMSIZ-1);
        ifr.ifr_name[IFNAMSIZ-1] = '\0';  // Ensure null-terminated
        
        // Get MAC address
        if(ioctl(fd, SIOCGIFHWADDR, &ifr) == 0){
            unsigned char *mac_addr = (unsigned char *)ifr.ifr_hwaddr.sa_data;
            // Check if all zeros
            int is_zero = 1;
            for(int i = 0; i < 6; i++){
                if(mac_addr[i] != 0){
                    is_zero = 0;
                    break;
                }
            }
            if(is_zero){
                DEBUG_LOG(fp, "  MAC Address: all zeros\n");
                fprintf(fp, "  MAC Address: NA\n");
            } else {
                DEBUG_LOG(fp, "  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
                fprintf(fp, "  MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            }
        } else {
            WARN_LOG(fp, "  Failed to get MAC address for %s\n", ifa->ifa_name);
        }
        
        if(ioctl(fd, SIOCGIFMTU, &ifr) == 0){
            DEBUG_LOG(fp, "  MTU: %d\n", ifr.ifr_mtu);
            fprintf(fp, "  MTU: %d\n", ifr.ifr_mtu);
        }
        // Show Netmask and Broadcast only for IPv4
        if(ifa->ifa_addr->sa_family == AF_INET){
            if(ioctl(fd, SIOCGIFNETMASK, &ifr) == 0){
                struct sockaddr_in *mask = (struct sockaddr_in *)&ifr.ifr_netmask;
                DEBUG_LOG(fp, "  Netmask: %s\n", inet_ntoa(mask->sin_addr));
                fprintf(fp, "  Netmask: %s\n", inet_ntoa(mask->sin_addr));
            }
            if(ioctl(fd, SIOCGIFBRDADDR, &ifr) == 0){
                struct sockaddr_in *brd = (struct sockaddr_in *)&ifr.ifr_broadaddr;
                DEBUG_LOG(fp, "  Broadcast: %s\n", inet_ntoa(brd->sin_addr));
                fprintf(fp, "  Broadcast: %s\n", inet_ntoa(brd->sin_addr));
            }
        }
        close(fd);
    }
    
    INFO_LOG(fp, "Processed %d network interfaces\n", interface_count);
    freeifaddrs(ifaddr);
    return 0;
}



