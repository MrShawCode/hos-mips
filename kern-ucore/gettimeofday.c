#include "linux_misc_struct.h"
#include <proc.h>
#include <string.h>

extern int ticks;
int ucore_gettimeofday(struct linux_timeval __user * tv,
		       struct linux_timezone __user * tz)
{
	struct linux_timeval ktv;
	ktv.tv_sec = ticks / 100;
	ktv.tv_usec = (ticks % 100) * 10000;
	
	if (!memcpy(tv, &ktv, sizeof(struct linux_timeval))) {
		
		return -1;
	}
	
	if (tz) {
		struct linux_timezone ktz;
		memset(&ktz, 0, sizeof(struct linux_timezone));
		
		if (!memcpy(tz, &ktz, sizeof(struct linux_timezone))) {
			
			return -1;
		}
		
	}
	return 0;
}

int do_clock_gettime(struct linux_timespec __user * time)
{
	struct linux_timespec ktv;
	ktv.tv_sec = ticks / 100;
	ktv.tv_nsec = (ticks % 100) * 10000000;
	
	if (!memcpy(time, &ktv, sizeof(struct linux_timespec))) {
		
		return -1;
	}
	
	return 0;
}
