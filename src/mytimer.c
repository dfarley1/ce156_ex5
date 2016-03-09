/*
 * mytimer.c
 *
 * This library lets you use absolute timers with select().
 *
 * See timertest.c for an example of its use.
 */

#include <time.h>
#include <limits.h>
#include "mytimer.h"
#include "myunp.h"

// Call this function to start a one-time timer.
// After this timer fires, it won't restart.
void timer_start(mytimer_t *timer,
                 int delay_in_seconds,
                 void (*callback)(time_t))
{
    timer->alarm_time   = time(NULL) + delay_in_seconds;
    timer->period       = 0;    // special value means non-periodic timer
    timer->callback     = callback;
}

// Call this function to start a periodic timer.
// After it fires, it will restart automatically.
void timer_start_periodic(mytimer_t *timer,
                          int delay_in_seconds,
                          void (*callback)(time_t))
{
    timer->alarm_time   = time(NULL) + delay_in_seconds;
    timer->period       = delay_in_seconds;
    timer->callback     = callback;
}

// Call this function to clear a running timer.
// After calling this function, the timer won't fire.
void timer_clear(mytimer_t *timer)
{
    timer->alarm_time = TIME_T_MAX;
}

// Call this function after select() to check each timer.
// If it's time for the timer to fire, it will call the callback function.
// If the timer is periodic, it will restart automatically.
void timer_check(mytimer_t *timer)
{
    time_t now = time(NULL);

    if (now > timer->alarm_time)
    {
        // Call the callback function.
        if (timer->callback != NULL) timer->callback(now);

        // Clear or restart the timer.
        if (timer->period == 0)
        {
            // clear the one-time timer
            timer->alarm_time = TIME_T_MAX;
        }
        else
        {
            // restart the periodic timer
            timer->alarm_time += timer->period;
        }
    }
}

// Call this function before calling tv_timer().
void tv_init(struct timeval *tv)
{
    tv->tv_usec = 0;
    tv->tv_sec  = LONG_MAX;
}

// Call this function for each timer before calling select().
// After it's been called for all timers, tv will have a timeout
// for the earliest timer. Then you can pass &tv to select().
void tv_timer(struct timeval *tv, mytimer_t *timer)
{
    // How many seconds until the timer's alarm goes off?
    time_t num_seconds = timer->alarm_time - time(NULL);

    // trigger past timer immediately
    if (num_seconds < 0) num_seconds = 0;
    
    // select() has an undocumented maximum allowed value for tv_sec.
    if (num_seconds > 100000000) num_seconds = 100000000;

    // use the earliest timer
    if (num_seconds < tv->tv_sec)
    {
        tv->tv_sec = (long) num_seconds;
    }
}
