#include "play.h"

/* Original controls, provisional cadence, scoring and unlock table. 8 ticks/s. */
int play_available(const Pet *p)
{
    static const int count[] = {0,1,2,4,6,0};
    return count[p->stage];
}

unsigned play_min_weight(const Pet *p)
{
    if (p->character==V3_LEGACY) return 1;
    /* V3 care chart (spudilike). Read the actual form, including elders
       which share the adult stage. Migration must not rewrite stored weight. */
    unsigned id=p->character;
    if (p->stage==BABY) return 5;
    if (p->stage==CHILD) return 10;
    if (p->stage==TEEN && id>=V3_OBOTCHI && id<=V3_HASHITAMATCHI)
        return (id-V3_OBOTCHI)%5<3?20:15;
    if (p->stage==ADULT && id>=V3_MAMETCHI && id<=V3_WARUSOTCHI)
        return (id-V3_MAMETCHI)%15<9?30:20;
    if (id==V3_OYAJITCHI) return 20;
    if (p->stage==ADULT) return 30;
    return 1;
}

static void next_round(Play *g, Pet *p)
{
    ++g->round;
    g->ticks = g->input = 0;
    g->answered=false;g->power=0;g->finish_ticks=0;g->round_won=false;
    g->target = pet_random(p)%3;
    if (g->type==BUMP || g->type==SPRINT) {
        /* Visible single-player rivals. The 2006 opponent weights are unknown. */
        g->rival=V3_MAMETCHI+(unsigned)(g->round*7+g->target)%30;
        journal_meet(p,g->rival);
    }
    g->double_note = pet_random(p)%3==0;
    g->hazard = pet_random(p)%4 == 0;
    if (g->type==FLAG && g->score<6) g->hazard=false;
    g->length = g->round + 1;
    if (g->length > 8) g->length = 8;
    for (int i=0;i<g->length;++i) g->pattern[i] = pet_random(p)%3;
}

void play_start(Play *g, Pet *p, enum Activity type)
{
    *g = (Play){.type=type, .lane=1};
    next_round(g,p);
}

static unsigned game_reward(const Play *g,Pet *p)
{
    static const unsigned get[]={0,10,20,30,50,70,90,150};
    static const int cut[]={3,21,36,51,66,85,100};
    static const unsigned bump[]={5,80,160,240,320,400,600,1200,2400};
    static const unsigned flag[]={4,8,10,20,30,50,70,100,150,300};
    static const unsigned heading[]={0,4,8,12,16,20,30,40,50,60,70,80,90,100,120,140,160,180,200,250,400};
    static const unsigned memory[]={5,10,30,70,120,180,280};
    static const unsigned sprint[]={5,10,30,50,70,90,120,200,400};
    unsigned n=g->score;
    switch (g->type) {
    case GET: {
        unsigned band=0;
        while (band<7 && g->score>=cut[band]) ++band;
        return get[band]*(p->stage<=CHILD?2:p->stage==TEEN?3:4)/2;
    }
    case BUMP: if (n==5 && p->weight>75 && p->weight<=90) return 180;
               return n?bump[n]*(p->weight<=50?4:p->weight<=75?3:p->weight<=90?2:1)/4:5;
    case FLAG: return flag[n/3]*(p->stage==TEEN?1:2);
    case HEADING: return heading[n];
    case MEMORY:return n==20?400:memory[n/3];
    case SPRINT: {
        static const unsigned extra[]={0,0,0,0,0,45,60,40,200};
        return sprint[n]+(extra[n]?pet_random(p)%(extra[n]+1):0);
    }
    default:return 0;
    }
}
static void finish(Play *g, Pet *p, bool won)
{
    static const int heart[]={36,4,9,8,6,4};
    g->done=true;g->won=won;
    unsigned before=p->points;
    p->points+=game_reward(g,p);
    if (p->points>9999) p->points=9999;
    g->earned=p->points-before;
    if (won) p->happy=pet_happy_max(p);
    else if (g->score>=heart[g->type] && p->happy<pet_happy_max(p)) ++p->happy;
    if (p->happy) p->happy_wait=0;
    if ((g->type==GET && g->score>=85) || (g->type==FLAG && g->score>=24)) {
        if (p->mental) --p->mental;
        if (p->stage_mental) --p->stage_mental;
    }
    if ((g->type==BUMP && g->score>=7) || (g->type==HEADING && won)) {
        if (p->physical) --p->physical;
        if (p->stage_physical) --p->stage_physical;
    }
    unsigned loss=won?3:(g->score>=heart[g->type] ||
                          ((g->type==SPRINT || g->type==BUMP) && g->score))?2:1;
    p->weight=p->weight>loss?p->weight-loss:1;
}

static void resolve(Play *g, Pet *p, bool success)
{
    if (!success) { finish(g,p,false); return; }
    static const int goals[]={100,8,27,20,20,8};
    if (g->type==GET) {
        if (!g->hazard) g->score+=g->double_note ? 2 : 1;
        if (g->score>100) g->score=100;
    } else ++g->score;
    if (g->score>=goals[g->type]) finish(g,p,true);
    else next_round(g,p);
}

static void show_finish(Play *g,bool won)
{
    g->round_won=won;
    g->finish_ticks=g->type==BUMP?BUMP_CLASH_TICKS:
                    g->type==HEADING?HEADING_JUMP_TICKS:SPRINT_FINISH_TICKS;
}

void play_tick(Play *g, Pet *p)
{
    if (g->done) return;
    if (g->finish_ticks) {
        if (!--g->finish_ticks) resolve(g,p,g->round_won);
        return;
    }
    ++g->ticks;
    switch (g->type) {
    case GET:
        if (g->ticks >= 12) resolve(g,p,(g->lane == g->target) != g->hazard);
        break;
    case FLAG:
        if (g->ticks>=12-(g->score/9)) {
            if (g->hazard) next_round(g,p);else finish(g,p,false);
        }
        break;
    case BUMP:
        if (g->answered) g->power=(g->ticks%12)/2;
        else if (g->ticks>=BUMP_ENTRY_TICKS+16) show_finish(g,false);
        break;
    case HEADING: if (g->ticks >= 13) finish(g,p,false); break;
    case MEMORY: if (g->ticks > g->length*5+64) finish(g,p,false); break;
    case SPRINT: if (g->ticks >= 40) show_finish(g,false); break;
    default: break;
    }
}

void play_key(Play *g, Pet *p, enum Key key)
{
    if (g->done || g->finish_ticks) return;
    switch (g->type) {
    case GET: case HEADING:
        if (key==KEY_A && g->lane>0) --g->lane;
        if (key==KEY_C && g->lane<2) ++g->lane;
        if (g->type==HEADING && key==KEY_B)
            show_finish(g,g->lane==g->target && g->ticks>=8);
        break;
    case BUMP:
        if (g->answered) show_finish(g,g->power>=(p->weight>75?2:3));
        else if (key==KEY_B && g->ticks>=BUMP_ENTRY_TICKS) {
            g->answered=true;g->ticks=0;
        }
        break;
    case FLAG:
        if (g->hazard || key==KEY_B) finish(g,p,false);
        else if (g->target==2) {
            g->input|=key==KEY_A?1:2;
            if (g->input==3) resolve(g,p,true);
        } else resolve(g,p,key==(g->target?KEY_C:KEY_A));
        break;
    case MEMORY:
        if (g->ticks < g->length*5) break;
        if ((int)key != g->pattern[g->input]) finish(g,p,false);
        else if (++g->input==g->length) resolve(g,p,true);
        break;
    case SPRINT:
        if (++g->input >= 16) show_finish(g,true);
        break;
    default: break;
    }
}
