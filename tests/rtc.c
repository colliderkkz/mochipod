#include "../src/core/rtc_guard.h"
#include "../src/storage/save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Pet child_at(uint32_t last)
{
    Pet p;
    pet_new(&p,last);
    p.setup=0;p.character=V3_LEGACY;p.generation=0;p.stage=CHILD;
    p.minutes=70;p.hunger=4;p.happy=4;
    return p;
}

static void normal_elapsed(void)
{
    uint32_t start=civil_minute(2026,12,31,23,57);
    const unsigned deltas[]={5,180,4320,31*1440};
    for (unsigned i=0;i<sizeof deltas/sizeof *deltas;++i) {
        Pet p=child_at(start),reference=p;
        assert(clock_check_start(&p,start+deltas[i],0,false)==CLOCK_OK);
        pet_clock(&p,start+deltas[i],0);
        for (unsigned t=1;t<=deltas[i];++t) pet_clock(&reference,start+t,0);
        assert(!memcmp(&p,&reference,sizeof p));
        assert((p.stage==DEAD)==(i>=2));
    }
}

static void abnormal(void)
{
    uint32_t today=civil_minute(2026,11,21,10,0);
    Pet p=child_at(today),original=p;
    assert(clock_check_start(&p,today-1,0,true)==CLOCK_BACKWARD);
    assert(clock_check_start(&p,0,0,true)==CLOCK_RESET);
    assert(clock_check_start(&p,UINT32_MAX,0,true)==CLOCK_INVALID);
    assert(clock_check_start(&p,today,60,true)==CLOCK_INVALID);
    assert(clock_check_start(&p,today+32u*1440,0,true)==CLOCK_GAP);
    uint32_t future=civil_minute(2099,10,1,10,0);
    assert(clock_check_start(&p,future,0,true)==CLOCK_OUTLIER);
    assert(!memcmp(&p,&original,sizeof p));
    p.last=future;
    assert(clock_check_start(&p,today,0,true)==CLOCK_BACKWARD);
    assert(clock_check_start(&p,future,0,false)==CLOCK_OUTLIER);
    p.last=216759;
    assert(clock_check_start(&p,today,0,false)==CLOCK_RESET);
    assert(!clock_can_count(&p,today,0) && !clock_can_resume(216759,0));
    p=original;
    assert(clock_can_count(&p,today+40*1440,0));
    pet_clock(&p,today+40*1440,0);
    assert(p.stage==DEAD);
    ClockAnchor a={today*60,UINT32_MAX-49};
    assert(clock_continuous(a,today,1,50,100));
    assert(!clock_continuous(a,today+60,0,50,100));
    assert(!clock_continuous(a,today-1,59,50,100));
    assert(!clock_continuous(a,today,0,500,100));
    pet_new(&p,today);p.setup=0;
    pet_clock(&p,today,10);pet_clock(&p,today,40);
    assert(p.egg_seconds==30 && clock_check_start(&p,today,35,true)==CLOCK_BACKWARD);
    p.clock_hold=CLOCK_BACKWARD;
    clock_rebase(&p,today-10,12);
    assert(!p.clock_hold && p.egg_seconds==30 && p.stage==EGG);
    pet_clock(&p,today-10,13);
    assert(p.egg_seconds==31 && p.stage==EGG);
}

static void check_copy(const char *path)
{
    FILE *f=fopen(path,"rb");
    assert(f);
    uint8_t bytes[RECORD_BYTES];
    unsigned len=(unsigned)fread(bytes,1,sizeof bytes,f);
    assert(!ferror(f) && fgetc(f)==EOF);
    fclose(f);
    uint32_t seq,n;
    Pet p,back;
    assert(save_unpack(bytes,len,&p,&seq));
    uint32_t rtc=civil_minute(2026,11,21,12,0);
    assert(clock_check_start(&p,rtc,0,false)==CLOCK_RESET);
    Pet old=p;
    p.clock_hold=CLOCK_RESET;
    save_pack(bytes,&p,seq+1);
    assert(save_unpack(bytes,sizeof bytes,&back,&n) && n==seq+1 && back.clock_hold==CLOCK_RESET);
    clock_rebase(&back,rtc,0);
    old.last=rtc;
    assert(!memcmp(&old,&back,sizeof old));
    uint8_t committed[RECORD_BYTES];
    save_pack(committed,&back,seq+2);
    for (unsigned size=0;size<RECORD_BYTES;++size)
        assert(!save_unpack(committed,size,&p,&n));
    assert(save_unpack(bytes,sizeof bytes,&p,&n) && p.clock_hold==CLOCK_RESET);
    assert(save_unpack(committed,sizeof committed,&p,&n) && save_newer(n,seq+1));
    assert(!p.clock_hold && clock_check_start(&p,rtc,0,true)==CLOCK_OK);
}

int main(int argc,char **argv)
{
    normal_elapsed();abnormal();
    for(int i=1;i<argc;++i) check_copy(argv[i]);
    printf("PASS normal minutes/hours/days and death, year boundary, reset, future, rollback, tick wrap, live jump, egg rebase; %d backup slots\n",argc-1);
    return 0;
}
