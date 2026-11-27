#include "../src/core/ui.h"
#include "../src/storage/save.h"
#include <stdio.h>
#include "../src/core/sound.h"
#include <assert.h>
#include <string.h>
#include "family-route.h"

static bool checkpoint(Pet *p,Ui *u,const char *scene)
{
    (void)u;
    uint8_t bytes[RECORD_BYTES];Pet loaded;uint32_t serial;
    assert(pet_valid(p));
    save_pack(bytes,p,9);
    assert(save_unpack(bytes,sizeof bytes,&loaded,&serial) && serial==9 && !memcmp(p,&loaded,sizeof loaded));
    if (p->generation==1 && !strcmp(scene,"offer")) {
        Pet alternate=loaded;
        assert(family_answer(&alternate,false) && !family_answer(&alternate,true));
        for (int i=0;i<8*1440 && !is_senior(&alternate);++i) {
            tend_family(&alternate);pet_advance(&alternate,alternate.last+1);
            if (alternate.family.phase==VISIT) assert(family_answer(&alternate,false));
        }
        assert(is_senior(&alternate) && alternate.stage==ADULT && pet_valid(&alternate));
        while(pet_sleeping(&alternate)) {tend_family(&alternate);pet_advance(&alternate,alternate.last+1);}
        assert(family_elder_offer(&alternate) && family_answer(&alternate,true));
        while(alternate.family.phase!=HANDOVER) {tend_family(&alternate);pet_advance(&alternate,alternate.last+1);}
        assert(!family_takeover(&alternate));pet_clock(&alternate,alternate.last+480,0);
        assert(family_takeover(&alternate) && !family_takeover(&alternate));
        for (int i=0;i<6;++i) setup_key(&alternate,KEY_B);
        for (int i=0;i<60;++i) {tend_family(&alternate);pet_advance(&alternate,alternate.last+1);}
        assert(alternate.character==V3_OYAJITCHI && pet_valid(&alternate));
    }
    if (!strcmp(scene,"birth") || !strcmp(scene,"departure")) {
        Pet held=loaded;
        held.clock_hold=3;Pet before=held;
        pet_clock(&held,held.last+8*1440,0);
        assert(!family_takeover(&held) && !family_answer(&held,true) && !memcmp(&held,&before,sizeof held));
    }
    if (!strcmp(scene,"offer")) {
        Pet refused=loaded;
        assert(family_answer(&refused,false) && !family_answer(&refused,true));
        for (int i=0;i<10;++i) {tend_family(&refused);pet_clock(&refused,refused.last+1,0);}
        assert(refused.family.phase==SINGLE);
        Pet timeout=loaded;
        for (int i=0;i<10;++i) {tend_family(&timeout);pet_clock(&timeout,timeout.last+1,0);}
        assert(timeout.family.phase==SINGLE);
    }
    *p=loaded;
    return true;
}

static void migrate_v9(const char *path,unsigned gen,unsigned phase)
{
    FILE *f=fopen(path,"rb");assert(f);
    uint8_t old[V9_BYTES],bytes[RECORD_BYTES];Pet p,again;uint32_t seq;
    assert(fread(old,1,sizeof old,f)==sizeof old && fgetc(f)==EOF && !fclose(f));
    assert(save_unpack(old,sizeof old,&p,&seq));
    assert(p.generation==gen && p.family.phase==phase && p.family.count==gen-1);
    assert(!p.sleep_ack && !p.potty_used && !p.last_call && !p.hungry_minutes && !p.ill_minutes);
    save_pack(bytes,&p,seq);
    /* Every old field, from sequence through memorials, remains bit-identical. */
    assert(!memcmp(bytes+8,old+8,V9_BYTES-12));
    assert(save_unpack(bytes,sizeof bytes,&again,&seq) && !memcmp(&p,&again,sizeof p));
    if (phase==WITH_BABY) assert(!family_answer(&again,true) && !family_takeover(&again));
}

static void child_medicine(void)
{
    uint8_t old[V11_BYTES],bytes[RECORD_BYTES];
    FILE *file=fopen("tests/fixtures/v11-offer.a","rb");
    assert(file && fread(old,1,sizeof old,file)==sizeof old && !fclose(file));
    Pet p,copy;
    uint32_t seq;
    assert(save_unpack(old,sizeof old,&p,&seq) && !p.family.child_doses);
    save_pack(bytes,&p,seq);
    assert(!memcmp(old+8,bytes+8,sizeof old-12));
    assert(family_answer(&p,true));
    p.last=civil_minute(2026,2,1,10,0);
    for (unsigned age=1;age<=59;++age) {
        tend_family(&p);pet_advance(&p,p.last+1);assert(!p.family.child_doses);
    }
    p.clock_hold=3;copy=p;pet_advance(&copy,p.last+5);
    assert(!memcmp(&p,&copy,sizeof p));
    p.clock_hold=0;pet_advance(&p,p.last+1);
    assert(p.family.child_doses==1 && !p.illness && pet_attention(&p));
    p.illness=2;
    unsigned bouts=p.stage_sickness;
    Ui u;ui_start(&u);u.page=HOME;u.icon=IC_MEDICINE;ui_command(&u,&p,CMD_OK);
    assert(u.page==NOTICE && u.detail && !strcmp(u.message,"Baby medicine"));
    assert(!p.family.child_doses && p.illness==2 && p.stage_sickness==bouts);
    assert(pet_care(&p,MEDICINE) && p.illness==1);
    assert(pet_care(&p,MEDICINE) && !p.illness);
    save_pack(bytes,&p,seq);
    assert(save_unpack(bytes,sizeof bytes,&copy,&seq));
    pet_advance(&copy,p.last);assert(!memcmp(&copy,&p,sizeof p));
    for(unsigned age=61;age<720;++age) {
        tend_family(&p);pet_advance(&p,p.last+1);assert(!p.family.child_doses);
    }
    pet_advance(&p,p.last+1);assert(p.family.child_doses==2);
    save_pack(bytes,&p,seq);assert(save_unpack(bytes,sizeof bytes,&copy,&seq));
    assert(copy.family.child_doses==2 && !memcmp(&copy,&p,sizeof p));
    while(p.family.phase!=HANDOVER) {
        /* Tend only the parent. Carry unresolved medicine into the new generation. */
        if (p.hunger<3) pet_care(&p,MEAL);
        if (p.happy<3) pet_care(&p,SNACK);
        if (p.mess) pet_care(&p,FLUSH);
        pet_advance(&p,p.last+1);assert(p.stage!=DEAD);
    }
    pet_advance(&p,p.last+480);
    p.shop.souvenirs=UINT32_MAX;
    assert(family_takeover(&p) && !p.family.child_doses && p.illness==2);
    for (unsigned n=1;n<=32;++n) {
        bool departs=n==2 || n==3 || (n>=7 && n<=9) || (n>=18 && n<=21);
        assert(((p.shop.souvenirs>>(n-1))&1u)==(unsigned)!departs);
    }
    assert(pet_valid(&p) && !family_takeover(&p));
    puts("PASS rc.1 wire fields identical; baby medicine crosses two boundaries, RTC hold, restore, separate parent and handover");
}

static void migration_copy(const char *path)
{
    FILE *f=fopen(path,"rb");
    uint8_t old[RECORD_BYTES],next[RECORD_BYTES];
    assert(f);
    size_t len=fread(old,1,sizeof old,f);
    assert(!ferror(f) && fgetc(f)==EOF && !fclose(f));
    Pet p,again;uint32_t seq,restored_seq;
    assert(save_unpack(old,len,&p,&seq));
    save_pack(next,&p,seq);
    assert(!memcmp(old+8,next+8,len-12));
    assert(save_unpack(next,sizeof next,&again,&restored_seq) && restored_seq==seq);
    assert(!memcmp(&p,&again,sizeof p));
    char name[6]={0};
    for (unsigned i=0;i<5;++i) name[i]=name_letter(p.pet_name,i);
    printf("PASS v%u seq=%u pet=%s form=%u stage=%u generation=%u RTC=%u hold=%u; old fields identical\n",
           old[4],seq,name,p.character,p.stage,p.generation,p.last,p.clock_hold);
}

int main(int argc,char **argv)
{
    for (int i=1;i<argc;++i) migration_copy(argv[i]);
    child_medicine();
    {
        FILE *file=fopen("tests/fixtures/v10-offer.a","rb");assert(file);
        uint8_t old[V10_BYTES],next[RECORD_BYTES];Pet pet;uint32_t seq;
        assert(fread(old,1,sizeof old,file)==sizeof old && !fclose(file));
        assert(save_unpack(old,sizeof old,&pet,&seq));
        assert(pet.journal.raised[pet.character]==65537 && !pet.journal.raised[pet.family.mate]);
        save_pack(next,&pet,seq);assert(!memcmp(next+8,old+8,sizeof old-12));
        Ui view;ui_start(&view);view.page=HOME;ui_tick(&view,&pet);
        assert(view.page==OFFER && pet.journal.seen[pet.family.mate/32]&(1u<<(pet.family.mate%32)));
        Feedback before=feedback_sample(&pet,&view);
        ui_command(&view,&pet,CMD_OK);
        assert(feedback_change(before,&pet,&view,CMD_OK)==MARRY_TONE);
        while (view.frames>25) ui_tick(&view,&pet);
        before=feedback_sample(&pet,&view);ui_tick(&view,&pet);
        assert(feedback_change(before,&pet,&view,CMD_NONE)==BIRTH_TONE);
        assert(view.page==WEDDING);uint32_t birth=pet.family.birth;
        ui_command(&view,&pet,CMD_OK);
        assert(view.page==HOME && pet.family.phase==WITH_BABY && pet.family.birth==birth);
        view.page=SETTINGS;view.choice=4;ui_command(&view,&pet,CMD_OK);
        assert(pet.sound_level==1);view.choice=2;ui_command(&view,&pet,CMD_OK);
        assert(pet.muted==1);save_pack(next,&pet,33);
        Pet reloaded;assert(save_unpack(next,sizeof next,&reloaded,&seq));
        assert(reloaded.muted==1 && reloaded.sound_level==1 && reloaded.family.birth==birth);
        puts("PASS sound transition on marriage and birth, mute/volume survive reload");
        puts("PASS v10 migration, journal provenance and skipped wedding commits one baby");
    }
    migrate_v9("tests/fixtures/v9-offer.a",1,VISIT);
    migrate_v9("tests/fixtures/v9-birth.a",3,WITH_BABY);
    migrate_v9("tests/fixtures/v9-history.a",4,SINGLE);
    puts("PASS historical v9 proposal, co-resident baby and fourth-generation lineage migrate with identical stored fields");
    Pet p;Ui u;
    assert(family_route(&p,&u,4,checkpoint));
    assert(p.family.records[0].name==3 && p.family.records[2].name==1);
    assert(family_route(&p,&u,11,checkpoint));
    assert(p.family.count==LINEAGE_LIMIT && p.family.records[0].name==10 && p.family.records[7].name==3);
    Pet dead=p;dead.stage=DEAD;
    pet_restart(&dead);
    assert(dead.memorial_count==1 && dead.memorial[0][0]==p.pet_name &&
           dead.memorial[0][1]==p.character && dead.memorial[0][2]==11 && pet_valid(&dead));
    puts("PASS generation 11 with lineage ring at capacity");
    puts("PASS egg -> 4th generation adult, three marriages, births, departures, history, refusal, seniors and Oyajitchi; reload at every checkpoint");
}
