#include "rtc_guard.h"

bool clock_can_resume(uint32_t minute,unsigned second)
{
    return minute<=MAX_CLOCK && second<60 && minute>=366u*1440u;
}

enum ClockIssue clock_check_start(const Pet *p,uint32_t minute,unsigned second,bool trusted)
{
    if (minute>MAX_CLOCK || second>=60) return CLOCK_INVALID;
    if (!clock_can_resume(minute,second) || !clock_can_resume(p->last,0)) return CLOCK_RESET;
    if (minute<p->last || (p->egg_sample && minute*60+second+1<p->egg_sample)) return CLOCK_BACKWARD;
    /* A first observation a decade beyond this release needs human review.
       An explicitly accepted baseline in that era remains usable. */
    uint32_t horizon=civil_minute(2037,1,1,0,0);
    if (minute>=horizon && (!trusted || p->last<horizon)) return CLOCK_OUTLIER;
    if (minute-p->last>31u*1440u) return CLOCK_GAP;
    return CLOCK_OK;
}

bool clock_continuous(ClockAnchor anchor,uint32_t minute,unsigned second,uint32_t tick,unsigned hz)
{
    if (!clock_can_resume(minute,second) || !hz) return false;
    uint32_t now=minute*60+second;
    if (now<anchor.seconds) return false;
    uint32_t rtc=now-anchor.seconds, elapsed=(tick-anchor.ticks)/hz;
    return rtc>=elapsed ? rtc-elapsed<=3 : elapsed-rtc<=3;
}

bool clock_can_count(const Pet *p,uint32_t minute,unsigned second)
{
    return clock_can_resume(minute,second) && clock_can_resume(p->last,0) &&
           minute>p->last && (!p->egg_sample || minute*60+second+1>=p->egg_sample);
}

void clock_rebase(Pet *p,uint32_t minute,unsigned second)
{
    p->last=minute;
    if (p->egg_sample) p->egg_sample=minute*60+second+1;
    p->clock_hold=CLOCK_OK;
}
