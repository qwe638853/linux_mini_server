#define _GNU_SOURCE
#include "../include/sysinfo.h"

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <net/if.h>
#include <arpa/inet.h>
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

int main(){
    get_hostname();
    get_local_time();
    return 0;
}

