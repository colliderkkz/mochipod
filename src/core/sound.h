#ifndef MOCHIPOD_SOUND_H
#define MOCHIPOD_SOUND_H
#include "ui.h"

enum Cue { SILENT, MOVE_TONE, OK_TONE, BACK_TONE, BITE_TONE, HAPPY_TONE,
           SICK_TONE, CALL_TONE, GAME_TONE, GROW_TONE, MARRY_TONE, BIRTH_TONE,
           DEATH_TONE, DRUM_TONE, MUSIC_TONE, LOST_TONE, FLAG_SHORT, FLAG_LONG, CUE_COUNT };
typedef struct {enum Cue cue;unsigned index;} Sound;
typedef struct {enum Page page; unsigned stage,form,illness,choice,icon,score,frames,round;bool alert,done;} Feedback;
Feedback feedback_sample(const Pet *p,const Ui *u);
enum Cue feedback_change(Feedback before,const Pet *p,const Ui *u,enum Command cmd);
unsigned sound_duration(enum Cue cue);
unsigned sound_note(Sound *s);
#endif
