#include "growth.h"

bool is_senior(const Pet *p)
{
    return p->character==V3_OJITCHI || p->character==V3_OTOKITCHI;
}

static bool regular_adult(unsigned id)
{
    return id>=V3_MAMETCHI && id<=V3_WARUSOTCHI;
}

/* Stable roster order is five groups of three for each parity. */
static unsigned partner_group(unsigned id)
{
    return ((id-V3_MAMETCHI)%15)/3;
}

void family_minute(Pet *p)
{
    Family *f=&p->family;
    if (p->character==V3_LEGACY || p->stage!=ADULT) return;
    unsigned time=p->last%1440;
    if (f->phase==WITH_BABY) {
        /* Exact bout times are unknown. Test only the crossing, so an old
           family save is never assigned an illness retroactively. */
        unsigned together=p->minutes-f->event_age;
        if (!f->child_doses && (together==60 || together==720))
            f->child_doses=together==60?1:2;
        if (!time && together>=1440) {
            f->phase=HANDOVER;
            f->event_age=480; /* unpaused minutes until the first 08:00 wake */
        }
        return;
    }
    if (f->phase==VISIT && p->minutes-f->event_age>=10) {
        f->phase=SINGLE; f->mate=0;
    }
    if (f->phase!=SINGLE || is_senior(p)) return;
    unsigned adult_age=p->minutes-p->stage_start;
    if (adult_age>=7u*1440 && p->character!=V3_OYAJITCHI) {
        p->character=p->sex?V3_OTOKITCHI:V3_OJITCHI;
        journal_raise(p,p->character,p->generation);p->stage_sickness=0;
        return;
    }
    if (p->generation>=9999 || adult_age<3u*1440 || (time!=630 && time!=900 && time!=1140)) return;
    unsigned group=pet_random(p)%5;
    if (f->count && f->records[0].method==1) group=FRAIL+pet_random(p)%2;
    f->mate=V3_MAMETCHI+(p->generation%2?0:15)+3*group+pet_random(p)%3;
    f->event_age=p->minutes;f->phase=VISIT;
}

bool family_elder_offer(Pet *p)
{
    if (!is_senior(p) || p->clock_hold || p->paused || p->stage!=ADULT ||
        pet_sleeping(p) || p->family.phase!=SINGLE || p->generation>=9999) return false;
    p->family.mate=p->sex?V3_OJITCHI:V3_OTOKITCHI;
    p->family.event_age=p->minutes;p->family.phase=VISIT;
    return true;
}

bool family_answer(Pet *p,bool accept)
{
    Family *f=&p->family;
    if (p->clock_hold || p->paused || p->stage!=ADULT || f->phase!=VISIT) return false;
    if (!accept) { f->phase=SINGLE;f->mate=0;return true; }
    f->child_sex=is_senior(p)?0:pet_random(p)%2;
    journal_meet(p,f->mate);
    journal_meet(p,f->child_sex?V3_TELETCHI_GIRL:V3_TELETCHI_BOY);
    f->birth=p->last;
    f->event_age=p->minutes;
    f->phase=WITH_BABY;
    return true;
}

bool family_takeover(Pet *p)
{
    Family *f=&p->family;
    if (p->clock_hold || p->paused || p->stage!=ADULT || f->phase!=HANDOVER || f->event_age || p->generation>=9999) return false;
    unsigned grade=is_senior(p)?GRADE_A:parent_grade(p->grade,partner_group(f->mate));
    unsigned sex=f->child_sex;
    if (f->count<LINEAGE_LIMIT) ++f->count;
    for (unsigned i=f->count-1;i>0;--i) f->records[i]=f->records[i-1];
    f->records[0]=(Ancestor){p->pet_name,p->character,f->mate,p->history,f->birth,p->sex,is_senior(p)?2u:1u,sex};
    f->phase=SINGLE;f->birth=0;f->event_age=0;f->mate=0;f->child_sex=0;
    p->shop.worn=0;
    p->shop.visited=0;
    /* One-based V3 souvenir numbers 2,3,7,8,9 and 18 through 21
       belong to the departing generation. Migration never applies this mask. */
    p->shop.souvenirs &= ~((3u<<1) | (7u<<6) | (15u<<17));
    ++p->generation;
    p->stage=BABY;p->character=sex?V3_TELETCHI_GIRL:V3_TELETCHI_BOY;
    p->grade=grade;p->sex=sex;p->history=0;p->minutes=1;p->awake=0;
    p->hunger=p->happy=p->mess=p->illness=p->dark=p->call=p->training=p->mistakes=p->neglected=0;
    p->physical=p->mental=p->hungry_wait=p->happy_wait=p->sick_wait=p->call_wait=p->light_wait=0;
    p->hungry_minutes=p->ill_minutes=p->last_call=p->potty_used=p->sleep_ack=0;
    p->snacks_today=p->cavities=0;
    p->illness=f->child_doses;
    f->child_doses=0;
    p->weight=5;p->pet_name=0;p->setup=REVEAL_BABY;
    record_form(p);
    return true;
}

bool family_valid(const Pet *p)
{
    const Family *f=&p->family;
    if (f->phase>HANDOVER || f->child_doses>2 || (f->child_doses && f->phase<WITH_BABY) || f->count>LINEAGE_LIMIT ||
        (f->count && f->count>=p->generation) || f->child_sex>1 ||
        f->event_age>p->minutes || f->birth>MAX_CLOCK) return false;
    if (f->phase==SINGLE && (f->mate || f->child_sex || f->birth)) return false;
    if (f->phase!=SINGLE && (p->character==V3_LEGACY || (p->stage!=ADULT && p->stage!=DEAD) ||
        (is_senior(p)? f->mate!=(p->sex?V3_OJITCHI:V3_OTOKITCHI) : !regular_adult(f->mate)))) return false;
    for (unsigned i=0;i<f->count;++i) {
        const Ancestor *a=&f->records[i];
        if (a->name>=11881376 || a->sex>1 || a->child_sex>1 || a->birth>MAX_CLOCK) return false;
        if (a->method==1) {
            if ((!regular_adult(a->form) && a->form!=V3_OYAJITCHI) || !regular_adult(a->mate)) return false;
        } else if (a->method!=2 || a->form!=(a->sex?V3_OTOKITCHI:V3_OJITCHI) ||
                   a->mate!=(a->sex?V3_OJITCHI:V3_OTOKITCHI)) return false;
    }
    return true;
}
