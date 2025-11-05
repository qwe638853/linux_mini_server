#pragma once
#include <stdio.h>
#include "debug.h"

int get_hostname(FILE *fp);
int get_local_time(FILE *fp);
int get_os_info(FILE *fp);
int get_memory_usage(FILE *fp);
int get_user_info(FILE *fp);
int get_disk_info(FILE *fp);
int get_env_info(FILE *fp);
int get_network_info(FILE *fp);