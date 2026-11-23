#include "sound.h"

/* Original MochiPod melodies. Each nonzero note occupies one UI beat. */
static const uint16_t notes[CUE_COUNT][5]={
    [MOVE_TONE]={440},[OK_TONE]={660,880},[BACK_TONE]={440,330},
    [BITE_TONE]={262,392},[HAPPY_TONE]={523,659,784},
    [SICK_TONE]={370,294,247},[CALL_TONE]={880,660,880},
    [GAME_TONE]={587},[GROW_TONE]={392,494,587,784},
    [MARRY_TONE]={523,659,587,784},[BIRTH_TONE]={784,988,1047},
    [DEATH_TONE]={392,330,294,196},[DRUM_TONE]={147,110,175,110},
    [MUSIC_TONE]={659,523,784,659},[LOST_TONE]={392,330},[FLAG_SHORT]={740},[FLAG_LONG]={740}
};
unsigned sound_duration(enum Cue cue)
{
    if (cue==FLAG_SHORT) return 45;
    if (cue==FLAG_LONG) return 125;
    return cue==MOVE_TONE?24:85;
}

unsigned sound_note(Sound *s)
{
    if ((unsigned)s->cue>=CUE_COUNT || s->index>=5) return 0;
    unsigned hz=notes[s->cue][s->index++];
    if (!hz) s->cue=SILENT;
    return hz;
}
Feedback feedback_sample(const Pet *p,const Ui *u)
{
    return (Feedback){u->page,p->stage,p->character,p->illness,(unsigned)u->choice,
                      (unsigned)u->icon,(unsigned)u->play.score,(unsigned)u->frames,(unsigned)u->play.round,
                      pet_attention(p),u->play.done};
}
enum Cue feedback_change(Feedback b,const Pet *p,const Ui *u,enum Command cmd)
{
    if (p->stage==DEAD && b.stage!=DEAD) return DEATH_TONE;
    if (p->character!=b.form) return p->stage==BABY?BIRTH_TONE:GROW_TONE;
    if (u->page==WEDDING) {
        if (b.page!=WEDDING) return MARRY_TONE;
        if (b.frames>24 && u->frames<=24) return BIRTH_TONE;
    }
    if (!b.illness && p->illness) return SICK_TONE;
    if (!b.alert && pet_attention(p)) return CALL_TONE;
    if (u->page==PLAYING) {
        if (!b.done && u->play.done) return u->play.won?HAPPY_TONE:LOST_TONE;
        if (u->play.type==FLAG && (b.page!=PLAYING || b.round!=(unsigned)u->play.round))
            return u->play.hazard?FLAG_SHORT:FLAG_LONG;
        if ((unsigned)u->play.score!=b.score || cmd==CMD_OK) return GAME_TONE;
    }
    if (u->page==USING_ITEM && b.page!=USING_ITEM) {
        if (u->item==DRUM) return DRUM_TONE;
        if (u->item==TRUMPET || u->item==GRAMOPHONE || u->item==MUSIC_DISC ||
            u->item==MUSIC || u->item==BOOM_BOX) return MUSIC_TONE;
        return HAPPY_TONE;
    }
    if (u->page==NOTICE && b.page!=NOTICE && u->care_scene) {
        if (u->care_scene==MEAL+1 || u->care_scene==SNACK+1) return BITE_TONE;
        if (u->care_scene==PRAISE+1) return HAPPY_TONE;
    }
    if (cmd==CMD_BACK || cmd==CMD_CANCEL) return BACK_TONE;
    if (cmd==CMD_OK) return OK_TONE;
    if (b.choice!=(unsigned)u->choice || b.icon!=(unsigned)u->icon) return MOVE_TONE;
    return SILENT;
}
