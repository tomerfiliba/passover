/*
 * High Performance Time
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "hptime.h"


static double hptime_cpu_freq_usec = 0;
static usec_t hptime_boot_time = 0;
static usec_t hptime_last_wall_time = 0;

static inline uint64_t hptime_get_cpu_cycles(void)
{
#ifdef __x86_64__
	unsigned int __a,__d;
	asm volatile("rdtsc" : "=a" (__a), "=d" (__d));
	return (((unsigned long)__a) | (((unsigned long)__d)<<32));
#else
	unsigned long long val = 0;
	__asm__ __volatile__("rdtsc" : "=A" (val));
	return val;
#endif
}

static inline errcode_t hptime_get_cpu_freq(double * out)
{
	char line[1000];
	double mhz = -1;
	FILE * f = fopen("/proc/cpuinfo", "r");

	if (f == NULL) {
		return ERR_HPTIME_FOPEN_FAILED;
	}

	while (fgets(line, sizeof(line) - 1, f) != NULL) {
		if (strstr(line, "cpu MHz") != NULL) {
			mhz = atof(strstr(line, ":") + 2);
			break;
		}
	}

	fclose(f);
	*out = mhz * 1000000;
	RETURN_SUCCESSFUL;
}

static inline usec_t hptime_get_usec_since_boot(void)
{
	return (usec_t)(hptime_get_cpu_cycles() / hptime_cpu_freq_usec);
}

static inline usec_t hptime_get_wall_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (unsigned long long)tv.tv_usec +
		((unsigned long long)1000000) * tv.tv_sec;
}

static inline usec_t hptime_get_fast_time(void)
{
	return hptime_boot_time + hptime_get_usec_since_boot();
}

errcode_t hptime_init(void)
{
	if (hptime_boot_time != 0) {
		RETURN_SUCCESSFUL; // already initialized
	}
	double freq;
	PROPAGATE(hptime_get_cpu_freq(&freq));
	hptime_cpu_freq_usec = freq / 1000000;
	hptime_last_wall_time = hptime_get_wall_time();
	hptime_boot_time = hptime_last_wall_time - hptime_get_usec_since_boot();
	RETURN_SUCCESSFUL;
}

errcode_t hptime_fini(void)
{
	RETURN_SUCCESSFUL;
}

inline usec_t hptime_get_time(void)
{
	usec_t curr_time = hptime_get_fast_time();

	if (hptime_last_wall_time + HPTIME_SYNC_INTERVAL < curr_time) {
		hptime_last_wall_time = hptime_get_wall_time();
		return hptime_last_wall_time;
	}

	return curr_time;
}

