#ifndef MOCHI_PLAY_H
#define MOCHI_PLAY_H
#include "game.h"
enum Activity { GET, BUMP, FLAG, HEADING, MEMORY, SPRINT, ACTIVITIES };
typedef struct {
    enum Activity type;
    int ticks, round, lane, target, score, power, input, length, pattern[8];
    unsigned earned,rival;
    int finish_ticks;
    bool round_won;
    bool double_note, hazard, answered, done, won;
} Play;
enum { BUMP_ENTRY_TICKS=8, BUMP_CLASH_TICKS=18, SPRINT_FINISH_TICKS=8, HEADING_JUMP_TICKS=4 };
void play_start(Play *g, Pet *p, enum Activity type);
void play_tick(Play *g, Pet *p);
void play_key(Play *g, Pet *p, enum Key key);
unsigned play_min_weight(const Pet *p);
int play_available(const Pet *p);
#endif
