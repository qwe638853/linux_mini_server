#pragma once
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/sysmacros.h>

int get_basic_sysinfo();

int get_hostname();
int get_local_time();
int get_uptime();
int get_memory_usage();
int get_disk_usage();
int get_network_usage();
int get_cpu_usage();
int get_cpu_temperature();
int get_cpu_load();
int get_cpu_frequency();
int get_cpu_cache();
int get_cpu_voltage();