#define _GNU_SOURCE
#include "../include/sysinfo.h"

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


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




int main(){
    get_hostname();
    get_local_time();
    get_os_info();
    get_memory_usage();
    get_user_info();
    get_disk_info();
    get_env_info();
    return 0;
}

