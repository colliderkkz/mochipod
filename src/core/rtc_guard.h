#ifndef MOCHI_RTC_GUARD_H
#define MOCHI_RTC_GUARD_H
#include "game.h"

enum ClockIssue { CLOCK_OK, CLOCK_INVALID, CLOCK_RESET, CLOCK_BACKWARD,
                  CLOCK_OUTLIER, CLOCK_GAP, CLOCK_LIVE_JUMP };
typedef struct { uint32_t seconds, ticks; } ClockAnchor;
enum ClockIssue clock_check_start(const Pet *p,uint32_t minute,unsigned second,bool trusted);
bool clock_continuous(ClockAnchor anchor,uint32_t minute,unsigned second,uint32_t tick,unsigned hz);
bool clock_can_resume(uint32_t minute,unsigned second);
bool clock_can_count(const Pet *p,uint32_t minute,unsigned second);
void clock_rebase(Pet *p,uint32_t minute,unsigned second);
#endif
