#include "game.h"
#include "growth.h"

static bool leap(int y) { return y % 4 == 0; } /* accepted years 2000..2099 */
int month_days(int year,int month)
{
    static const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month<1 || month>12) return 0;
    return lengths[month-1]+(month==2 && leap(year));
}
uint32_t civil_minute(int year, int month, int day, int hour, int minute)
{
    static const int lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (year < 2000 || year > 2099 || month < 1 || month > 12 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || day < 1 ||
        day > lengths[month-1] + (month == 2 && leap(year))) return UINT32_MAX;
    int days = (year-2000)*365 + (year-1997)/4 + day-1;
    for (int m=1; m<month; ++m) days += lengths[m-1] + (m==2 && leap(year));
    return (uint32_t)((days*24 + hour)*60 + minute);
}

void civil_date(uint32_t minute,unsigned *year,unsigned *month,unsigned *day)
{
    unsigned days=minute/1440;
    *year=2000+(days/1461)*4;
    days%=1461;
    while (days>=365u+leap(*year)) {days-=365u+leap(*year);++*year;}
    *month=1;
    while (days>=(unsigned)month_days(*year,*month)) {days-=month_days(*year,*month);++*month;}
    *day=days+1;
}

void pet_new(Pet *p, uint32_t clock)
{
    *p = (Pet){.last=clock, .weight=5, .seed=clock+1, .setup=1, .birthday_month=1, .birthday_day=1, .character=V3_EGG, .generation=1};
}

static const uint32_t place[]={1,26,676,17576,456976};
char name_letter(uint32_t code,unsigned i) { return 'A'+(code/place[i])%26; }

void setup_key(Pet *p,enum Key key)
{
    if (!p->setup || key==KEY_C) return;
    if (key==KEY_B) {
        if (p->setup==REVEAL_BABY) p->setup=8;
        else if (p->setup==7 && p->character==V3_EGG) p->setup=0;
        else if (++p->setup>12) p->setup=0;
        return;
    }
    setup_adjust(p,1);
}

void setup_adjust(Pet *p,int delta)
{
    if (!p->setup || p->setup==REVEAL_BABY) return;
    if (p->setup<=2) {
        uint32_t *field=p->setup==1 ? &p->birthday_month : &p->birthday_day;
        int limit=p->setup==1 ? 12 : month_days(2000,p->birthday_month);
        *field=1+((int)*field-1+limit+delta)%limit;
        unsigned days=month_days(2000,p->birthday_month);
        if (p->birthday_day>days) p->birthday_day=days;
    } else {
        unsigned i=(p->setup-3)%5;
        uint32_t *name=p->setup<8 ? &p->user_name : &p->pet_name;
        int old=name_letter(*name,i)-'A';
        *name=*name-old*place[i]+((old+26+delta)%26)*place[i];
    }
}

void pet_restart(Pet *p)
{
    Pet old=*p;
    pet_new(p,old.last);
    p->birthday_month=old.birthday_month;
    p->birthday_day=old.birthday_day;
    p->user_name=old.user_name;
    p->muted=old.muted;p->sound_level=old.sound_level;p->journal=old.journal;
    p->setup=(old.setup && old.setup<=7) ? 1 : 0;
    p->seed=old.seed;
    p->memorial_count=old.memorial_count;
    for (unsigned i=0;i<3;++i)
        for (unsigned j=0;j<3;++j) p->memorial[i][j]=old.memorial[i][j];
    if (old.stage==DEAD) {
        if (p->memorial_count<3) ++p->memorial_count;
        for (int i=2;i>0;--i)
            for (unsigned j=0;j<3;++j) p->memorial[i][j]=p->memorial[i-1][j];
        p->memorial[0][0]=old.pet_name;p->memorial[0][1]=old.character;
        p->memorial[0][2]=old.generation;
    }
}

uint32_t pet_random(Pet *p)
{
    p->seed = p->seed * 1664525u + 1013904223u;
    return p->seed >> 8;
}

/* Stage thresholds and all physiological rates are provisional. */
static const struct { const char *label; unsigned begin; } stages[] = {
    {"EGG",0}, {"BABY",1}, {"CHILD",61}, {"TEEN",1501}, {"ADULT",4381}, {"DEAD",0}
};

const char *pet_stage(const Pet *p) { return is_senior(p) && p->stage!=DEAD ? "SENIOR" : stages[p->stage].label; }

/* Roster order contains three serious and two normal teens per parity,
   then five groups of three adults per parity. Use identity even on migrated saves. */
static bool serious_teen(const Pet *p)
{
    return p->character>=V3_OBOTCHI && p->character<=V3_HASHITAMATCHI &&
           (p->character-V3_OBOTCHI)%5<3;
}

static unsigned temperament(const Pet *p)
{
    if (p->character>=V3_MAMETCHI && p->character<=V3_WARUSOTCHI)
        return (p->character-V3_MAMETCHI)%15/3;
    return STUBBORN;
}

SleepHours pet_hours(const Pet *p)
{
    static const SleepHours adults[]={{480,1260},{540,1320},{480,1320},{540,1260},{600,1335}};
    if (p->stage<CHILD || p->stage>ADULT) return (SleepHours){0,0};
    if (p->character==V3_LEGACY) return (SleepHours){480,1200};
    if (is_senior(p)) return (SleepHours){420,1200};
    if (p->stage==ADULT) return adults[temperament(p)];
    if (p->stage==TEEN) return serious_teen(p)?(SleepHours){540,1200}:(SleepHours){600,1260};
    bool later=p->character==V3_KUCHITAMATCHI || p->character==V3_MOHITAMATCHI;
    return (SleepHours){later?540:480,1200};
}

bool pet_sleeping(const Pet *p)
{
    if (p->stage==BABY)
        return p->character!=V3_LEGACY && !p->setup && p->minutes>=31 && p->minutes<36;
    SleepHours hours=pet_hours(p);
    unsigned t=p->last%1440;
    return hours.bed && (t>=hours.bed || t<hours.wake ||
           (p->sleep_ack && t+30>=hours.bed));
}

static unsigned need_rate(const Pet *p) {return p->stage==BABY?10:60;}

bool pet_needs_toilet(const Pet *p)
{
    if (p->character==V3_LEGACY || p->setup || p->stage==DEAD || p->stage==EGG ||
        p->potty_used || pet_sleeping(p)) return false;
    unsigned cycle=2*need_rate(p);
    return p->awake%cycle>=cycle-2;
}

enum Routine pet_routine(const Pet *p)
{
    if (p->character==V3_LEGACY || p->stage<CHILD || p->stage>ADULT ||
        p->paused || p->clock_hold || p->setup) return ROAM;
    unsigned time=p->last%1440;
    if (time==0 || (time>=1350 && time<1353)) {
        unsigned year,month,day;
        civil_date(p->last,&year,&month,&day);
        if (month==1 && day==1 && !time) return NEW_YEAR;
        if (month==12 && day==24 && time>=1350 && pet_sleeping(p)) return SANTA_VISIT;
    }
    if (is_senior(p) || pet_sleeping(p) || p->illness || p->call) return ROAM;
    SleepHours h=pet_hours(p);
    unsigned t=p->last%1440,g=temperament(p);
    if (t==690 || t==840 || t==990) {
        unsigned year,month,day;civil_date(p->last,&year,&month,&day);
        if (month==p->birthday_month && day==p->birthday_day) return CAKE;
        if (month==2) return SNOWMAN;
        if (month==4 && day>=8 && day<=10 && t==990) return BUNNY;
        if ((month==8 && day>=15) || (month==9 && day<=8)) return SCHOOL;
        if (month==10 && day==31) return PUMPKIN;
        if (month==11 && day>=18 && day<=26) return FEAST;
        if ((month==11 && day==30) || (month==12 && day<=25)) return TREE;
    }
    bool serious=p->stage==TEEN?serious_teen(p):p->stage==ADULT && g==SERIOUS;
    if (p->character!=V3_OYAJITCHI &&
        ((t>=h.bed-30 && t<h.bed) || (!serious && t==h.wake))) return YAWN;
    if (p->stage==ADULT && ((g==SERIOUS && (t==570 || t==1170)) ||
        (g==NORMAL && (t==660 || t==1230)) || (g==FRAIL && (t==660 || t==1110)))) return BRUSH;
    if (p->stage>=TEEN && ((serious && t==1110) ||
        (t==1170 && ((p->stage==TEEN && !serious) || g==NORMAL || p->character==V3_OYAJITCHI)) ||
        (p->stage==ADULT && g==NAUGHTY_ADULT && t==1230))) return BATH;
    /* Unverified idle frequency: deterministic so repainting cannot consume growth RNG. */
    if ((p->awake+p->character)%37 || p->character==V3_OYAJITCHI) return ROAM;
    if (p->stage==CHILD) return ROLL;
    /* Both documented idle actions can appear without sampling the growth RNG. */
    if (((p->awake+p->character)/37)%2) return p->stage==TEEN?RUN:CLOSE_UP;
    if (serious) return DANCE;
    if (p->character==V3_ANDROTCHI) return SIGNAL;
    if (p->stage==TEEN || g==NORMAL || g==FRAIL) return SING;
    return g==STUBBORN?WATCH:RUN;
}

bool pet_attention(const Pet *p)
{
    if (p->stage==EGG || p->stage==DEAD || p->setup) return false;
    if (p->character==V3_LEGACY) return p->illness || (!pet_sleeping(p) && (!p->hunger || !p->happy || p->call));
    if (pet_sleeping(p)) return !p->dark && !p->sleep_ack && p->light_wait<60;
    return p->family.child_doses || (p->illness && p->sick_wait<15) || (!p->hunger && p->hungry_wait<15) ||
           (!p->happy && p->happy_wait<15) || p->call;
}

static void expire_need(Pet *p,bool active,uint32_t *elapsed,unsigned limit,uint32_t *category)
{
    if (!active) { *elapsed=0; return; }
    if (*elapsed>=limit) return;
    if (++*elapsed==limit) {
        if (*category<9999) ++*category;
        if (p->character!=V3_LEGACY) {
            uint32_t *stage_count=category==&p->physical?&p->stage_physical:&p->stage_mental;
            if (*stage_count<9999) ++*stage_count;
        }
        if (p->mistakes<9999) ++p->mistakes;
    }
}

static void fall_ill(Pet *p,bool toothache)
{
    if (p->illness) return;
    p->illness=2;p->cavities=toothache;
    if (++p->stage_sickness>=4) p->stage=DEAD;
}

static void step(Pet *p)
{
    bool slept = pet_sleeping(p);
    ++p->last;
    if (p->last%1440==0) p->snacks_today=0;
    if (p->stage == DEAD) return;
    if (p->family.phase==HANDOVER) {
        if (p->family.event_age) --p->family.event_age;
        return;
    }
    ++p->minutes;
    if (p->character==V3_LEGACY) {
        if (p->stage<ADULT && p->minutes>=stages[p->stage+1].begin) ++p->stage;
    } else evolve_pet(p);
    family_minute(p);
    if (p->family.phase==HANDOVER) return;
    unsigned minute=p->last%1440;
    if (!minute || minute==900 || minute==1140) {
        p->sold=0;
        p->shop.shelf_seed=(p->seed^p->minutes)|1u;
    }
    bool sleeping=pet_sleeping(p);
    if (sleeping) {
        p->shop.worn=0;
        if (p->dark) p->sleep_ack=1;
    }
    expire_need(p,sleeping && !p->dark && !p->sleep_ack,&p->light_wait,60,&p->mental);
    if (sleeping) return;
    if (slept || p->sleep_ack) {p->dark=0;p->sleep_ack=0;}
    ++p->awake;
    unsigned rate=need_rate(p);
    if (p->awake % rate == 0 && p->hunger) --p->hunger;
    if (p->awake % (rate+5) == 0 && p->happy) --p->happy;
    bool legacy=p->character==V3_LEGACY;
    if (p->awake%(rate*2)==0) {
        unsigned limit=legacy?4:8;
        if (!p->potty_used && p->mess<limit) ++p->mess;
        p->potty_used=0;
    }
    /* A 2006 Teletchi log records a praise call after the nap, about
       forty minutes after naming. The exact offset is reconstructed. */
    if (!legacy && p->stage==BABY && p->minutes==41 && !p->illness && !p->call)
        p->call=p->last_call=CRYING;
    if (p->awake%180==0 && (legacy || p->stage<ADULT) && p->hunger && p->happy) {
        p->call=legacy?pet_random(p)%2+1:p->last_call==CRYING?NAUGHTY:CRYING;
        p->last_call=p->call;
    }
    if (legacy) {
        if (!p->hunger || !p->happy || p->mess>=3 || p->illness) {
            ++p->neglected;
            if (p->neglected>=120 && !p->illness) p->illness=2;
            if (p->neglected>=720) p->stage=DEAD;
        } else p->neglected=0;
    } else {
        p->hungry_minutes=p->hunger?0:p->hungry_minutes+1;
        if (!p->illness && (p->mess==8 || p->hungry_minutes==120 ||
            (p->stage==BABY && p->minutes==16))) fall_ill(p,false);
        p->ill_minutes=p->illness?p->ill_minutes+1:0;
        if (p->hungry_minutes>=720 || p->ill_minutes>=720) p->stage=DEAD;
    }
    expire_need(p,!p->hunger,&p->hungry_wait,15,&p->physical);
    expire_need(p,!p->happy,&p->happy_wait,15,&p->mental);
    expire_need(p,p->illness>0,&p->sick_wait,15,&p->physical);
    expire_need(p,p->call!=QUIET,&p->call_wait,15,p->call==CRYING ? &p->mental : &p->physical);
    if (p->call_wait==15) { p->call=QUIET; p->call_wait=0; }
}

void pet_advance(Pet *p, uint32_t clock)
{
    if (p->clock_hold || clock > MAX_CLOCK || clock <= p->last) return;
    if (p->setup || p->paused || p->character==V3_EGG) {
        if (clock/1440!=p->last/1440) p->snacks_today=0;
        p->last=clock;return;
    }
    if (clock - p->last > WEEK) p->last = clock - WEEK;
    while (p->last < clock && p->stage != DEAD) step(p);
    p->last = clock;
}

/* The RTC minute alone can change one second after confirming the username. */
void pet_clock(Pet *p,uint32_t minute,unsigned second)
{
    if (p->clock_hold || minute>MAX_CLOCK || second>=60 || minute<p->last) return;
    if (p->character==V3_EGG && !p->setup) {
        uint32_t sample=minute*60+second+1;
        if (sample>=p->egg_sample) {
            if (p->egg_sample && !p->paused) {
                uint32_t elapsed=sample-p->egg_sample;
                unsigned left=HATCH_SECONDS-p->egg_seconds;
                p->egg_seconds+=elapsed<left ? elapsed : left;
            }
            p->egg_sample=sample;
        }
        if (p->egg_seconds==HATCH_SECONDS) {
            p->sex=pet_random(p)%2;
            p->character=p->sex ? V3_TELETCHI_GIRL : V3_TELETCHI_BOY;
            p->stage=BABY;
            p->minutes=1;
            record_form(p);
            p->setup=REVEAL_BABY;
            p->egg_sample=0;
            p->egg_seconds=0;
        }
    }
    pet_advance(p,minute);
}

unsigned pet_happy_max(const Pet *p) { return p->character==V3_LEGACY?4u:5u; }
static unsigned hunger_max(const Pet *p) { return p->character==V3_LEGACY?4u:6u; }

bool pet_care(Pet *p, enum Care care)
{
    if (p->setup || p->stage == EGG || p->stage == DEAD) return false;
    if (care==LIGHT) {
        SleepHours h=pet_hours(p);
        unsigned t=p->last%1440;
        if (!p->dark && !p->sleep_ack && p->character!=V3_LEGACY &&
            h.bed && t<h.bed && t+30>=h.bed) {
            p->sleep_ack=1;
            if (p->happy<pet_happy_max(p)) ++p->happy;
        }
        p->dark^=1;
        if (p->dark && pet_sleeping(p)) p->sleep_ack=1;
        return true;
    }
    if (pet_sleeping(p)) return false;
    if (p->character!=V3_LEGACY && p->illness && (care==MEAL || care==SNACK)) return false;
    switch (care) {
    case MEAL:
        if (p->hunger >= hunger_max(p) ||
            (p->character!=V3_LEGACY && p->call==NAUGHTY)) return false;
        ++p->hunger;p->hungry_minutes=0;
        p->hungry_wait=0;
        if (p->weight < 99) ++p->weight;
        break;
    case SNACK:
        if (p->character!=V3_LEGACY && (p->stage==CHILD || p->stage==TEEN)) {
            if (p->snacks_today<255) ++p->snacks_today;
            /* Daily threshold reconstructed; the original exact count is unverified. */
            if (p->snacks_today%8==0) fall_ill(p,true);
        }
        if (p->happy < pet_happy_max(p)) ++p->happy;
        p->happy_wait=0;
        p->weight = p->weight < 98 ? p->weight + 2 : 99;
        break;
    case FLUSH:
        if (pet_needs_toilet(p)) {
            p->potty_used=1;
            p->happy=p->happy+2<pet_happy_max(p)?p->happy+2:pet_happy_max(p);p->happy_wait=0;
        } else if (!p->mess) return false;
        p->mess=0;break;
    case MEDICINE:
        if (p->family.phase==WITH_BABY && p->family.child_doses) {
            --p->family.child_doses;
            break;
        }
        if (!p->illness) return false;
        if (!--p->illness) {p->sick_wait=0;p->ill_minutes=0;p->cavities=0;}
        break;
    case PRAISE: case TIME_OUT:
        if (p->call != (care == PRAISE ? CRYING : NAUGHTY)) {
            if (p->character==V3_LEGACY || care!=TIME_OUT || !p->happy) return false;
            --p->happy;
            return true;
        }
        if (p->character!=V3_LEGACY) {
            if (care==PRAISE && p->happy<pet_happy_max(p)) ++p->happy;
            if (care==TIME_OUT && p->happy) --p->happy;
        }
        p->call=QUIET; p->call_wait=0;
        if (p->training < 9) ++p->training;
        break;
    default: return false;
    }
    if (p->hunger && p->happy && p->mess < 3 && !p->illness) p->neglected=0;
    return true;
}

/* Two participant-recorded sequences, sampled for the prototype. See v3-reference. */
const char *pet_character(const Pet *p)
{
    static const char *const paths_by_stage[2][6]={
        {"EGG","TELETCHI","MIZUTAMATCHI","OBOTCHI","WOOLTCHI","DEAD"},
        {"EGG","TELETCHI","TAMATCHI","PIRORIROTCHI","MASKTCHI","DEAD"}
    };
    if (p->character!=V3_LEGACY && p->stage!=DEAD) return v3_characters[p->character].name;
    return paths_by_stage[p->route][p->stage];
}

bool pet_valid(const Pet *p)
{
    if (p->character>=V3_COUNT || p->egg_seconds>=HATCH_SECONDS ||
        p->egg_sample>(MAX_CLOCK+1)*60u) return false;
    if (p->character==V3_LEGACY) {
        if (p->generation || p->egg_sample || p->egg_seconds || p->setup>12) return false;
    } else {

        if (p->character==V3_EGG) {
            if (p->stage!=EGG || p->setup>7 || p->minutes || (p->setup && p->egg_sample)) return false;
        } else {
            if (p->egg_sample || p->egg_seconds ||
                (p->setup && (p->setup<8 || p->stage!=BABY))) return false;
        }
    }
    return shop_valid(p) && family_valid(p) && growth_valid(p) && p->last<=MAX_CLOCK && p->minutes<=MAX_CLOCK && p->awake<=p->minutes &&
        p->stage<=DEAD && p->hunger<=hunger_max(p) && p->happy<=pet_happy_max(p) && p->weight>=1 &&
        p->weight<=99 && p->mess<=(p->character==V3_LEGACY?4u:8u) && p->sleep_ack<=1 && p->potty_used<=1 &&
        p->last_call<=NAUGHTY && p->hungry_minutes<=720 && p->ill_minutes<=720 && p->illness<=2 && p->training<=9 &&
        p->mistakes<=9999 && p->neglected<=720 && p->call<=NAUGHTY &&
        p->physical<=9999 && p->mental<=9999 && p->hungry_wait<=15 &&
        p->happy_wait<=15 && p->sick_wait<=15 && p->call_wait<=15 && p->light_wait<=60 &&
        p->setup<=REVEAL_BABY && p->sex<=1 && p->paused<=1 && p->muted<=1 &&
        p->birthday_month>=1 && p->birthday_month<=12 && p->birthday_day>=1 &&
        p->birthday_day<=(unsigned)month_days(2000,p->birthday_month) &&
        p->pet_name<11881376 && p->user_name<11881376 && p->route<=1 && p->pies<=15 && p->balls<=30 && p->drums<=30 && p->tickets<=30 &&
        p->balls+p->drums+p->tickets<=30 && p->sold<16 && p->dark<=1 && p->points<=9999;
}
