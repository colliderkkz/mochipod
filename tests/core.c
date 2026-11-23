#include "../src/core/ui.h"
#include "../src/storage/save.h"
#include "../src/render/lcd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Pet baby(void)
{
    Pet p;
    pet_new(&p,civil_minute(2006,2,1,10,0));
    p.character=V3_LEGACY; p.generation=0; p.setup=0;
    assert(p.stage==EGG && !pet_care(&p,MEAL));
    pet_advance(&p,p.last+1);
    assert(p.stage==BABY && p.minutes==1);
    return p;
}

static void care_test(void)
{
    Pet p=baby();
    for (unsigned i=1;i<=4;++i) {
        assert(pet_care(&p,MEAL) && p.hunger==i);
        assert(pet_care(&p,SNACK) && p.happy==i);
    }
    assert(!pet_care(&p,MEAL) && p.weight==17);
    pet_advance(&p,p.last+20);
    assert(p.hunger==2 && p.happy==3 && p.mess==1);
    assert(pet_care(&p,FLUSH) && p.mess==0);
    p.call=CRYING;
    assert(!pet_care(&p,TIME_OUT) && pet_care(&p,PRAISE) && p.training==1);
    p.call=NAUGHTY;
    assert(pet_care(&p,TIME_OUT) && p.training==2);
    pet_advance(&p,p.last+190);
    assert(p.illness==2 && pet_attention(&p) && p.mistakes>0);
    assert(pet_care(&p,MEDICINE) && p.illness==1);
    assert(pet_care(&p,MEDICINE) && !p.illness);
    pet_advance(&p,p.last+3*1440);
    assert(p.stage==DEAD);
    Ui u; ui_start(&u); ui_command(&u,&p,CMD_OK); ui_command(&u,&p,CMD_BACK);
    for (int i=0;i<4;i++) ui_command(&u,&p,CMD_NEXT);
    ui_command(&u,&p,CMD_OK); ui_command(&u,&p,CMD_NEXT); ui_command(&u,&p,CMD_OK);
    ui_command(&u,&p,CMD_NEXT); ui_command(&u,&p,CMD_OK);
    assert(p.stage==EGG && !p.minutes && !p.points);
}

static void growth_test(void)
{
    for (unsigned route=0;route<2;++route) {
        Pet p=baby();
        p.route=route;
        unsigned observed=0;
        for (int i=0;i<4400;++i) {
            pet_advance(&p,p.last+1);
            if (!pet_sleeping(&p)) {
                if (p.hunger<2) pet_care(&p,MEAL);
                if (p.happy<2) pet_care(&p,SNACK);
                pet_care(&p,FLUSH);
            }
            if (p.minutes==61) { assert(p.stage==CHILD); observed=p.route; assert(observed==route); }
            if (p.minutes==1501) assert(p.stage==TEEN && p.route==observed);
            if (p.minutes==4381) assert(p.stage==ADULT && p.route==observed);
        }
        assert(strcmp(pet_character(&p),observed?"MASKTCHI":"WOOLTCHI")==0);
    }
}

static void clock_test(void)
{
    assert(civil_minute(2000,1,1,0,0)==0);
    assert(civil_minute(2099,12,31,23,59)==MAX_CLOCK);
    assert(civil_minute(2024,3,1,0,0)-civil_minute(2024,2,28,0,0)==2880);
    assert(civil_minute(2023,2,29,0,0)==UINT32_MAX);
    Pet p=baby();
    p.stage=CHILD;
    p.last=civil_minute(2026,11,20,19,59);
    pet_advance(&p,p.last+1);
    assert(pet_sleeping(&p) && !pet_care(&p,SNACK) && pet_care(&p,LIGHT));
    Pet split=p;
    pet_advance(&p,p.last+720);
    for (int i=0;i<720;++i) pet_advance(&split,split.last+1);
    assert(!pet_sleeping(&p) && !p.dark && !memcmp(&p,&split,sizeof p));
    pet_advance(&p,p.last); pet_advance(&p,p.last-80); pet_advance(&p,UINT32_MAX);
    assert(!memcmp(&p,&split,sizeof p));
    uint32_t old=p.minutes;
    pet_advance(&p,MAX_CLOCK);
    assert(p.minutes-old<=WEEK && p.last==MAX_CLOCK && p.stage==DEAD);
}

static void storage_test(void)
{
    Pet a=baby(),b={0}; uint8_t data[RECORD_BYTES]; uint32_t seq=0;
    a.pies=4; a.points=290; a.route=1;
    save_pack(data,&a,100);
    assert(save_unpack(data,sizeof data,&b,&seq) && seq==100 && !memcmp(&a,&b,sizeof a));
    pet_advance(&b,a.last);
    assert(!memcmp(&a,&b,sizeof a));
    for (unsigned i=0;i<sizeof data;++i) {
        data[i]^=16;
        assert(!save_unpack(data,sizeof data,&b,&seq));
        data[i]^=16;
        assert(!save_unpack(data,i,&b,&seq));
    }
    a.happy=5; save_pack(data,&a,3);
    assert(!save_unpack(data,sizeof data,&b,&seq));
    assert(save_newer(0,UINT32_MAX) && !save_newer(2,2));
}

/* Independent Python struct/zlib fixtures, including a future version with valid CRC. */
static void legacy_test(void)
{
    const unsigned sizes[]={80,92,124,152,164,164};
    const uint32_t crcs[]={0x5b08c803u,0x1af5a7a4u,0x55cafdfcu,0x985bfbecu,0xb14c3bf3u,0x96aa693fu};
    for (unsigned version=1;version<=6;++version) {
        unsigned n=sizes[version-1];
        uint8_t data[RECORD_BYTES]={'M','P','V','3'};
        data[4]=(uint8_t)version; data[8]=42;
        data[12]=0xe8; data[13]=3; data[32]=5;
        if (version>=3) { data[92]=2; data[96]=29; }
        for (unsigned i=0;i<4;++i) data[n-4+i]=(uint8_t)(crcs[version-1]>>(i*8));
        Pet p; uint32_t serial=0;
        assert(save_unpack(data,n,&p,&serial)==(version<=5));
        if (version<=5) {
            assert(p.last==1000 && p.weight==5 && serial==42);
            assert(p.birthday_day==(version>=3 ? 29u : 1u));
        }
    }
}

static void economy_test(void)
{
    Pet p=baby();
    p.last=civil_minute(2026,11,1,12,0); /* regular-price Sunday */
    assert(!buy_goods(&p,PIE));
    p.points=300;
    assert(buy_goods(&p,PIE) && p.points==150 && p.pies==1 && !buy_goods(&p,PIE));
    assert(eat_pie(&p) && !p.pies && !eat_pie(&p));
    p.last=civil_minute(2026,11,1,14,59);
    pet_advance(&p,p.last+1);
    assert(!p.sold && p.shop.shelf_seed);p.shop.shelf_seed=0;
    assert(buy_goods(&p,PIE) && p.points==0 && p.pies==1);
    p.pies=15; p.points=300; p.sold=0;
    assert(!buy_goods(&p,PIE) && p.points==300);
    p.points=5000;
    assert(buy_goods(&p,BALL) && !use_item(&p,BALL));
    p.stage=CHILD;
    assert(use_item(&p,BALL) && p.balls==1);
    assert(buy_goods(&p,DRUM) && !use_item(&p,DRUM));
    p.stage=ADULT;
    assert(use_item(&p,DRUM) && buy_goods(&p,HAWAII));
    assert(use_item(&p,HAWAII) && !use_item(&p,HAWAII));
}

static void games_test(void)
{
    Pet p=baby(); p.stage=ADULT;
    assert(play_available(&p)==6);
    Play bout;play_start(&bout,&p,BUMP);
    play_key(&bout,&p,KEY_B);assert(!bout.answered);
    for(int i=0;i<BUMP_ENTRY_TICKS;++i) play_tick(&bout,&p);
    play_key(&bout,&p,KEY_B);assert(bout.answered);
    for(int i=0;i<8;++i) play_tick(&bout,&p);
    play_key(&bout,&p,KEY_A);assert(bout.round_won && bout.finish_ticks==BUMP_CLASH_TICKS);
    for(int i=0;i<BUMP_CLASH_TICKS;++i) {play_key(&bout,&p,KEY_C);play_tick(&bout,&p);}
    assert(bout.score==1 && bout.round==2 && !bout.power);
    play_start(&bout,&p,SPRINT);
    for(int i=0;i<16;++i) play_key(&bout,&p,(enum Key)(i%3));
    assert(bout.round_won && bout.finish_ticks==SPRINT_FINISH_TICKS && !bout.score);
    for(int i=0;i<SPRINT_FINISH_TICKS;++i) play_tick(&bout,&p);
    assert(bout.score==1 && !bout.input);
    for (int type=0;type<ACTIVITIES;++type) {
        Play g; play_start(&g,&p,(enum Activity)type);
        while (!g.done) {
            switch(type) {
            case GET: case HEADING:
                while (g.lane<g.target) play_key(&g,&p,KEY_C);
                while (g.lane>g.target) play_key(&g,&p,KEY_A);
                if (type==GET && g.hazard) play_key(&g,&p,g.lane ? KEY_A : KEY_C);
                if (type==HEADING && g.ticks==8) play_key(&g,&p,KEY_B);
                break;
            case BUMP:
                if (!g.answered || g.power>=3) play_key(&g,&p,KEY_B);
                break;
            case FLAG:
                if (!g.hazard) {
                    if (g.target==2) {play_key(&g,&p,KEY_A);play_key(&g,&p,KEY_C);}
                    else play_key(&g,&p,g.target?KEY_C:KEY_A);
                }
                break;
            case MEMORY:
                if (g.ticks>=g.length*5) play_key(&g,&p,(enum Key)g.pattern[g.input]);
                break;
            case SPRINT: play_key(&g,&p,KEY_A); break;
            }
            play_tick(&g,&p);
            assert(g.round<500);
        }
        assert(g.won && p.points>0);
        uint32_t paid=p.points;
        play_tick(&g,&p); play_key(&g,&p,KEY_A);
        assert(p.points==paid);
        play_start(&g,&p,(enum Activity)type);
        if (type==GET) g.lane=g.hazard ? g.target : (g.target+1)%3;
        if (type==BUMP) { play_key(&g,&p,KEY_B); play_key(&g,&p,KEY_B); }
        if (type==FLAG) play_key(&g,&p,KEY_B);
        for (int i=0;i<200 && !g.done;++i) play_tick(&g,&p);
        assert(g.done && !g.won);
    }
}

static void ui_test(void)
{
    Pet p=baby(); Ui u; Display d,e;
    ui_start(&u); assert(u.page==TITLE); ui_command(&u,&p,CMD_OK);
    draw_frame(&d,&p,&u);
    for (int i=0;i<4;++i) ui_tick(&u,&p);
    draw_frame(&e,&p,&u); assert(memcmp(&d,&e,sizeof d));
    ui_command(&u,&p,CMD_OK); assert(u.page==STATUS);
    ui_command(&u,&p,CMD_BACK); ui_command(&u,&p,CMD_NEXT);
    ui_command(&u,&p,CMD_OK); ui_command(&u,&p,CMD_OK); ui_command(&u,&p,CMD_OK);
    assert(u.page==NOTICE && p.hunger==1);
    for (int i=0;i<12;++i) ui_tick(&u,&p);
    assert(u.page==DISH && u.icon==IC_FEED);
}

static void profile_test(void)
{
    Pet p;
    pet_new(&p,5000);
    p.character=V3_LEGACY; p.generation=0;
    pet_advance(&p,5010);
    assert(p.minutes==0 && p.last==5010);
    setup_key(&p,KEY_A); setup_key(&p,KEY_B);
    setup_key(&p,KEY_A); setup_key(&p,KEY_B);
    setup_key(&p,KEY_A);
    for (int i=0;i<5;++i) setup_key(&p,KEY_B);
    setup_key(&p,KEY_A); setup_key(&p,KEY_A);
    for (int i=0;i<5;++i) setup_key(&p,KEY_B);
    assert(!p.setup && p.birthday_month==2 && p.birthday_day==2 &&
           name_letter(p.user_name,0)=='B' && name_letter(p.pet_name,0)=='C');
    p.paused=1;
    pet_advance(&p,6010);
    assert(p.minutes==0 && p.last==6010);
    p.paused=0;
    pet_advance(&p,6024);
    assert(p.physical==0 && p.mental==0);
    pet_advance(&p,6025);
    assert(p.physical==1 && p.mental==1);
    pet_advance(&p,6026);
    assert(p.physical==1 && p.mental==1);
    pet_restart(&p);
    assert(!p.setup && p.character==V3_EGG && name_letter(p.user_name,0)=='B');
}

int main(void)
{
    legacy_test(); profile_test(); care_test(); growth_test(); clock_test(); storage_test();
    economy_test(); games_test(); ui_test();
    puts("PASS care, growth, clock, storage, economy, six games, UI");
    return 0;
}
