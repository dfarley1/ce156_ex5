/*
 * mytimer.h
 *
 * This library lets you use absolute timers with select().
 *
 * See timertest.c for an example of its use.
 */

#include "myunp.h"

#define TIME_T_MAX (sizeof(time_t) == 4 ? 0x7fffffff : 0x7fffffffffffffffL)

typedef struct
{
    time_t  alarm_time;
    long    period;
    void    (*callback)(time_t);
} mytimer_t;

#define TIMER_INIT {TIME_T_MAX, 0, NULL}

void timer_start(mytimer_t *timer, int delay_in_seconds, void (*)(time_t));
void timer_start_periodic(mytimer_t *timer, int delay_in_seconds, void (*)(time_t));
void timer_clear(mytimer_t *timer);
void timer_check(mytimer_t *timer);
void tv_init(struct timeval *tv);
void tv_timer(struct timeval *tv, mytimer_t *timer);
