/* One week at production rates. Only elapsed RTC minutes and player actions are driven. */
#include "../src/core/ui.h"
#include "../src/storage/save.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

enum Action { FOOD_ACTION,SNACK_ACTION,CLEAN_ACTION,HEAL_ACTION,TRAIN_ACTION,LIGHT_ACTION,
              GAME_ACTION,BUY_ACTION,USE_ACTION,NO_ACTION };
typedef struct {
    unsigned actions[NO_ACTION],previous,repeats,sessions,quiet,longest,run,quiet_end,asleep;
    unsigned games[ACTIVITIES],won,earned,spent,growth,marriages,departures,max_gap,last_visit;
} Day;
static void count(Day *d,enum Action a)
{
    d->repeats+=d->previous==(unsigned)a;d->previous=a;++d->actions[a];
}
static void visit(Pet *p,Day *d,unsigned visit_id)
{
    ++d->sessions;
    unsigned gap=p->last-d->last_visit;
    if (gap>d->max_gap) d->max_gap=gap;
    d->last_visit=p->last;
    if (p->family.phase==VISIT && family_answer(p,true)) ++d->marriages;
    if (p->family.phase==HANDOVER) {
        if (!family_takeover(p)) return;
        ++d->departures;
        for(int i=0;i<6;++i) setup_key(p,KEY_B);
    }
    if (pet_sleeping(p)) {
        if (!p->dark && pet_care(p,LIGHT)) count(d,LIGHT_ACTION);
        return;
    }
    while (p->illness && pet_care(p,MEDICINE)) count(d,HEAL_ACTION);
    while (p->hunger<4 && pet_care(p,MEAL)) count(d,FOOD_ACTION);
    if (p->mess || pet_needs_toilet(p)) {
        if (pet_care(p,FLUSH)) count(d,CLEAN_ACTION);
    }
    if (p->call && pet_care(p,p->call==CRYING?PRAISE:TIME_OUT)) count(d,TRAIN_ACTION);
    if (p->happy<3 && pet_care(p,SNACK)) count(d,SNACK_ACTION);
    Ui u;ui_start(&u);u.page=GAMES;
    unsigned game=visit_id%play_available(p);
    u.choice=(int)game;ui_command(&u,p,CMD_OK);
    if (u.page==PLAYING) {
        ++d->games[game];count(d,GAME_ACTION);
        unsigned before=p->points;
        /* Screen-directed responses; sometimes react too late or miss a cue. */
        for (int step=0;step<3000 && !u.play.done;++step) {
            Play *g=&u.play;
            bool slip=(g->round+visit_id)%17==0;
            if (game==GET || game==HEADING) {
                int target=g->hazard && game==GET?(g->target+1)%3:g->target;
                if (slip) target=(target+1)%3;
                if (g->lane!=target) ui_command(&u,p,g->lane<target?CMD_RIGHT:CMD_LEFT);
                if (game==HEADING && g->ticks==9) ui_command(&u,p,CMD_OK);
            } else if (game==BUMP && (!g->answered || g->power>=(slip?1:4))) ui_command(&u,p,CMD_OK);
            else if (game==FLAG && !g->hazard) {
                int target=g->target;
                ui_command(&u,p,slip?CMD_OK:target==1?CMD_RIGHT:CMD_LEFT);
                if (!slip && target==2) ui_command(&u,p,CMD_RIGHT);
            } else if (game==MEMORY && g->ticks>=g->length*5) {
                int k=(g->pattern[g->input]+(slip?1:0))%3;
                ui_command(&u,p,((enum Command[]){CMD_LEFT,CMD_OK,CMD_RIGHT})[k]);
            } else if (game==SPRINT && (!slip || step%4==0)) ui_command(&u,p,CMD_OK);
            ui_tick(&u,p);
        }
        assert(u.play.done);d->won+=u.play.won;d->earned+=p->points-before;
    }
    if (visit_id%2==0) {
        /* Try the four visible offers and buy at most one per visit. */
        for (unsigned i=0;i<4;++i) {
            unsigned id=shelf_item(p,(i+visit_id)%4),before=p->points;
            if (!buy_goods(p,id)) continue;
            if (getenv("MOCHI_TRACE_SHOP"))
                printf("BUY minute=%u role=%s item=%s gate=%s\n",p->last,pet_character(p),goods_names[id],
                       item_gate(p,id)?item_gate(p,id):"ready");
            count(d,BUY_ACTION);d->spent+=before-p->points;break;
        }
    }
    for (unsigned i=0;i<GOODS_COUNT;++i) {
        unsigned id=(i+visit_id)%GOODS_COUNT;
        if (goods_owned(p,id) && use_item(p,id)) {count(d,USE_ACTION);break;}
    }
}
int main(int argc,char **argv)
{
    uint32_t start=civil_minute(2028,2,26,8,0);
    Pet p;pet_new(&p,start);
    for (int i=0;i<7;++i) setup_key(&p,KEY_B);
    pet_clock(&p,start,0);pet_clock(&p,start+1,0);
    for (int i=0;i<6;++i) setup_key(&p,KEY_B);
    assert(p.stage==BABY && !p.setup);
    unsigned days=argc>1 && !strcmp(argv[1],"30")?30:7;
    unsigned visits=0,last_visit=p.last,total_births=0,total_departures=0;
    for (unsigned day=1;day<=days;++day) {
        Day d={.previous=NO_ACTION,.last_visit=last_visit};
        for (unsigned n=0;n<1440;++n) {
            unsigned old=p.character;
            unsigned time=p.last%1440;
            if ((day==1 && n<60 && n%10==0) || time==480 || time==570 ||
                time==720 || time==900 || time==1080 || time==1170 || time==1260) visit(&p,&d,visits++);
            d.asleep+=pet_sleeping(&p);
            if (!pet_sleeping(&p) && !pet_attention(&p) && !p.mess && !pet_needs_toilet(&p) &&
                pet_routine(&p)==ROAM && p.family.phase==SINGLE) {
                ++d.quiet;if (++d.run>d.longest) {d.longest=d.run;d.quiet_end=time;}
            } else d.run=0;
            pet_clock(&p,p.last+1,0);d.growth+=p.character!=old;
            assert(p.stage!=DEAD && pet_valid(&p));
        }
        uint8_t bytes[RECORD_BYTES];Pet back;uint32_t serial;
        save_pack(bytes,&p,day);assert(save_unpack(bytes,sizeof bytes,&back,&serial) && !memcmp(&back,&p,sizeof p));
        p=back;last_visit=d.last_visit;total_births+=d.marriages;total_departures+=d.departures;
        printf("EVENTS day=%u generation=%u births=%u handovers=%u visit_gap=%u\n",day,p.generation,d.marriages,d.departures,d.max_gap);
        printf("DAY %u %s sessions=%u actions=",day,pet_character(&p),d.sessions);
        for(int i=0;i<NO_ACTION;++i) printf("%s%u",i?",":"",d.actions[i]);
        printf(" games=");
        for(int i=0;i<ACTIVITIES;++i) printf("%s%u",i?",":"",d.games[i]);
        printf(" wins=%u earned=%u spent=%u repeat=%u quiet=%u longest=%u quiet_end=%u asleep=%u growth=%u\n",
               d.won,d.earned,d.spent,d.repeats,d.quiet,d.longest,d.quiet_end,d.asleep,d.growth);
    }
    if (days==30) {
        assert(p.generation>=3 && p.generation==1+total_departures && total_births>=total_departures);
        puts("PASS 30 days, 30 disk-format round trips, births and generation handovers");return 0;
    }
    assert(p.stage==ADULT && p.generation==1 && p.minutes==10081);
    puts("PASS seven production-rate days across leap day, seven save/reopens, original generation preserved");
}
