#include "../src/core/rtc_guard.h"
#include "../src/core/growth.h"
#include "../src/storage/save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Pet child(uint32_t t)
{
    Pet p;pet_new(&p,t);
    p.character=V3_MIZUTAMATCHI;p.setup=0;p.stage=CHILD;p.minutes=61;
    p.hunger=p.happy=4;p.history=V3_TELETCHI_BOY;record_form(&p);
    assert(pet_valid(&p));
    return p;
}
static Pet restored(Pet p)
{
    uint8_t bytes[RECORD_BYTES];Pet back;uint32_t serial;
    save_pack(bytes,&p,44);assert(save_unpack(bytes,sizeof bytes,&back,&serial));
    assert(serial==44 && !memcmp(&p,&back,sizeof p));return back;
}
static void overnight(uint32_t bedtime)
{
    Pet p=child(bedtime-1);
    assert(!pet_sleeping(&p) && pet_routine(&p)==YAWN);
    pet_clock(&p,bedtime,0);assert(pet_sleeping(&p) && pet_attention(&p));
    assert(pet_care(&p,LIGHT) && p.dark && !pet_attention(&p));
    Pet split=p;
    pet_clock(&p,bedtime+719,0);
    for(unsigned i=0;i<719;++i) {
        pet_clock(&split,split.last+1,0);assert(pet_sleeping(&split));
        if (i==390) split=restored(split);
    }
    assert(!memcmp(&p,&split,sizeof p) && p.hunger==4 && p.awake==0 && !p.mistakes);
    p=restored(p);pet_clock(&p,bedtime+720,0);
    assert(!pet_sleeping(&p) && p.awake==1 && !p.dark && !p.sleep_ack);
    Pet late=child(bedtime-1);pet_clock(&late,bedtime+60,0);
    assert(late.mental==1 && !pet_attention(&late));pet_clock(&late,bedtime+200,0);
    assert(late.mental==1);
    Pet early=child(bedtime-30);early.happy=2;
    assert(pet_care(&early,LIGHT) && pet_sleeping(&early) && early.happy==3);
    assert(pet_care(&early,LIGHT) && !early.dark && pet_sleeping(&early));
    assert(pet_care(&early,LIGHT) && early.happy==3);
    early=restored(early);early.clock_hold=CLOCK_BACKWARD;Pet held=early;
    pet_clock(&early,bedtime+720,0);assert(!memcmp(&early,&held,sizeof early));
}
static void schedules(void)
{
    for(unsigned id=V3_MIZUTAMATCHI;id<=V3_OYAJITCHI;++id) {
        Pet p=child(0);p.character=id;
        p.stage=id<=V3_MOHITAMATCHI?CHILD:id<=V3_HASHITAMATCHI?TEEN:ADULT;
        SleepHours s=pet_hours(&p);
        p.last=s.bed-1;assert(!pet_sleeping(&p));p.last=s.bed;assert(pet_sleeping(&p));
        p.last=1440+s.wake-1;assert(pet_sleeping(&p));++p.last;assert(!pet_sleeping(&p));
    }
    static const struct {unsigned id,stage,wake,bed;} timetable[]={
        {V3_MIZUTAMATCHI,CHILD,480,1200},{V3_MOHITAMATCHI,CHILD,540,1200},
        {V3_OBOTCHI,TEEN,540,1200},{V3_NIKATCHI,TEEN,600,1260},
        {V3_MAMETCHI,ADULT,480,1260},{V3_KUCHIPATCHI,ADULT,540,1320},
        {V3_OJITCHI,ADULT,420,1200},{V3_OTOKITCHI,ADULT,420,1200},
        {V3_OYAJITCHI,ADULT,600,1335}
    };
    for(unsigned i=0;i<sizeof timetable/sizeof *timetable;++i) {
        Pet role=child(0);role.character=timetable[i].id;role.stage=timetable[i].stage;
        SleepHours hours=pet_hours(&role);
        assert(hours.wake==timetable[i].wake && hours.bed==timetable[i].bed);
    }
    Pet p=child(0);p.stage=ADULT;p.character=V3_OYAJITCHI;
    assert(pet_hours(&p).bed==1335 && pet_hours(&p).wake==600);
    p.stage=BABY;p.character=V3_TELETCHI_GIRL;
    for(unsigned age=1;age<=60;++age) {p.minutes=age;assert(pet_sleeping(&p)==(age>=31 && age<36));}
}
static void hygiene(void)
{
    Pet p=child(720);p.awake=118;p.minutes=180;
    assert(pet_needs_toilet(&p));p.happy=1;
    assert(pet_care(&p,FLUSH) && p.happy==3 && !pet_care(&p,FLUSH));
    p=restored(p);pet_clock(&p,p.last+2,0);assert(!p.mess && !p.potty_used);
    p.awake=239;p.mess=7;pet_clock(&p,p.last+1,0);
    assert(p.mess==8 && p.illness && !pet_care(&p,SNACK));
    assert(pet_care(&p,FLUSH) && pet_care(&p,MEDICINE) && pet_care(&p,MEDICINE));
    assert(!p.illness && !p.mess);
}
int main(void)
{
    schedules();hygiene();
    Pet winter=child(civil_minute(2026,12,24,22,29));
    winter.dark=1;
    for (unsigned i=0;i<5;++i) {
        assert((pet_routine(&winter)==SANTA_VISIT)==(i>=1 && i<=3));
        assert(winter.dark && pet_sleeping(&winter));
        winter=restored(winter);pet_advance(&winter,winter.last+1);
    }
    winter.last=civil_minute(2026,12,31,23,59);
    pet_advance(&winter,winter.last+1);
    assert(pet_routine(&winter)==NEW_YEAR && winter.dark);
    winter.clock_hold=2;assert(pet_routine(&winter)==ROAM);
    winter.clock_hold=0;pet_advance(&winter,winter.last+1);assert(pet_routine(&winter)==ROAM);
    const struct {unsigned month,day,time;enum Routine scene;} dates[]={
        {2,28,690,SNOWMAN},{4,8,990,BUNNY},{4,10,990,BUNNY},{8,15,840,SCHOOL},
        {9,8,990,SCHOOL},{10,31,840,PUMPKIN},{11,18,690,FEAST},{11,26,990,FEAST},
        {11,30,840,TREE},{12,25,690,TREE},{1,1,840,CAKE}};
    for (unsigned i=0;i<sizeof dates/sizeof *dates;++i) {
        winter.last=civil_minute(2026,dates[i].month,dates[i].day,dates[i].time/60,dates[i].time%60);
        assert(pet_routine(&winter)==dates[i].scene);
        ++winter.last;assert(pet_routine(&winter)!=dates[i].scene);
    }
    Pet infant=child(720);infant.stage=BABY;infant.character=V3_TELETCHI_BOY;
    infant.minutes=40;infant.stage_start=1;infant.history=V3_TELETCHI_BOY;
    infant=restored(infant);pet_advance(&infant,721);
    assert(infant.call==CRYING && pet_care(&infant,PRAISE) && infant.training==1);
    infant=restored(infant);pet_advance(&infant,722);assert(!infant.call && !pet_care(&infant,PRAISE));
    Pet refusing=child(720);refusing.call=NAUGHTY;refusing.hunger=1;
    Pet before=refusing;assert(!pet_care(&refusing,MEAL) && !memcmp(&before,&refusing,sizeof before));
    assert(pet_care(&refusing,TIME_OUT) && pet_care(&refusing,MEAL));
    assert(refusing.hunger==2 && refusing.training==1);
    Pet tooth=child(720);
    for(unsigned bout=1;bout<=4;++bout) {
        for(unsigned n=0;n<8;++n) assert(pet_care(&tooth,SNACK));
        assert(tooth.cavities && tooth.illness && tooth.stage_sickness==bout);
        tooth=restored(tooth);
        if(bout<4) {
            assert(pet_care(&tooth,MEDICINE) && pet_care(&tooth,MEDICINE) && !tooth.cavities);
        } else assert(tooth.stage==DEAD);
    }
    Pet holiday=child(civil_minute(2026,10,31,14,0));assert(pet_routine(&holiday)==PUMPKIN);
    holiday.last=civil_minute(2026,4,9,16,30);assert(pet_routine(&holiday)==BUNNY);
    ++holiday.last;assert(pet_routine(&holiday)!=BUNNY);
    Pet full=child(720);
    assert(pet_care(&full,MEAL) && pet_care(&full,MEAL) && !pet_care(&full,MEAL));
    assert(pet_care(&full,SNACK) && full.happy==5 && full.hunger==6);
    full=restored(full);pet_clock(&full,full.last+65,0);
    assert(full.happy==4 && full.hunger==5);
    unsigned training=full.training;
    assert(pet_care(&full,TIME_OUT) && full.happy==3 && full.training==training);
    full.illness=1;full.balls=1;assert(!use_item(&full,BALL) && full.balls==1);
    overnight(civil_minute(2028,2,28,20,0));
    overnight(civil_minute(2028,2,29,20,0));
    overnight(civil_minute(2026,12,31,20,0));
    puts("PASS 47 child/teen/adult/senior schedules, baby nap, early and late lights, clock hold, overnight reload and wake, leap day/month/year, toilet and medicine");
}
