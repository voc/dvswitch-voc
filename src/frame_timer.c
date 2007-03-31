// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>

#include "frame_timer.h"

static timer_t frame_timer;

void init_frame_timer(void)
{
    sigset_t sigset_alarm;
    sigemptyset(&sigset_alarm);
    sigaddset(&sigset_alarm, SIGALRM);
    if (pthread_sigmask(SIG_BLOCK, &sigset_alarm, NULL) != 0)
    {
	perror("FATAL: pthread_sigmask");
	exit(1);
    }

    // On Linux, CLOCK_MONOTONIC matches the kernel interval timer
    // (resolution is controlled by HZ) and there is no
    // CLOCK_MONOTONIC_HR.
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC, &res) == -1)
    {
	perror("FATAL: clock_get_res");
	exit(1);
    }
    if (res.tv_sec != 0 || (unsigned)res.tv_nsec > frame_time_ns_525_60)
	fprintf(stderr, 
		"WARNING: CLOCK_MONOTONIC resolution is too low"
		" (%lu.%09lus)\n",
		(unsigned long)res.tv_sec, (unsigned long)res.tv_nsec);

    struct sigevent event = {
	.sigev_notify = SIGEV_SIGNAL,
	.sigev_signo =  SIGALRM
    };
    if (timer_create(CLOCK_MONOTONIC, &event, &frame_timer) != 0)
    {
	perror("FATAL: timer_create");
	exit(1);
    }
}

void set_frame_timer(unsigned interval_ns)
{
    struct itimerspec interval;
    interval.it_interval.tv_sec = 0;
    interval.it_interval.tv_nsec = interval_ns;
    interval.it_value = interval.it_interval;
    if (timer_settime(frame_timer, 0, &interval, NULL) != 0)
    {
	perror("FATAL: timer_settime");
	exit(1);
    }
}

void wait_frame_timer(void)
{
    sigset_t sigset_alarm;
    sigemptyset(&sigset_alarm);
    sigaddset(&sigset_alarm, SIGALRM);
    int dummy;
    sigwait(&sigset_alarm, &dummy);
}
