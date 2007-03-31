// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_FRAME_TIMER_H
#define DVSWITCH_FRAME_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialise timer in a disarmed state.  Must be done in the first
// thread before any more threads are created.
void init_frame_timer(void);

// Arm the timer and set its interval.
void set_frame_timer(unsigned interval_ns);

// Normal frame periods for "PAL" (625/50) and "NTSC" (525/60).
static const unsigned frame_time_ns_625_50 = 1000000000 / 25;
static const unsigned frame_time_ns_525_60 = 1001000000 / 30;

// Wait for the timer to expire.  The timer must be armed; otherwise
// this will wait forever!
void wait_frame_timer(void);

#ifdef __cplusplus
}
#endif

#endif // !defined(DVSWITCH_FRAME_TIMER_H)
