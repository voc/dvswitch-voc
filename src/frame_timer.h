// Copyright 2007 Ben Hutchings and Tore Sinding Bekkedal.
// See the file "COPYING" for licence details.

#ifndef DVSWITCH_FRAME_TIMER_H
#define DVSWITCH_FRAME_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialise timer in a disarmed state.  Must be done in the first
// thread before any more threads are created.
void frame_timer_init(void);

// Arm the timer and set the period between ticks.
void frame_timer_set(unsigned period_ns);

// Normal frame periods for "PAL" (625/50) and "NTSC" (525/60).
static const unsigned frame_time_ns_625_50 = 1000000000 / 25;
static const unsigned frame_time_ns_525_60 = 1001000000 / 30;

// Wait until at least 1 tick has occurred since the timer was armed
// or this function was last called.  Return the number of ticks that
// have occurred.
int frame_timer_wait(void);

#ifdef __cplusplus
}
#endif

#endif // !defined(DVSWITCH_FRAME_TIMER_H)
