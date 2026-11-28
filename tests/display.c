/* Run with MOCHI_LAYOUT_TEST to check every raster and text bounding box.
   Pass an existing output directory to also export representative native-size PGM frames. */
#include "../src/render/lcd.h"
#include <stdio.h>
#include "../src/core/sound.h"
#include <assert.h>
#include <string.h>

static Display display;
static Pet pet;
static Ui ui;
static int frames;
static const char *output;

static void sample(const char *label,int index)
{
    if (!output) return;
    char path[512];
    snprintf(path,sizeof(path),"%s/%s-%02d.pgm",output,label,index);
    FILE *file=fopen(path,"wb");
    assert(file);
    fprintf(file,"P5\n%d %d\n255\n",LCD_COLS,LCD_ROWS);
    for (int y=0;y<LCD_ROWS;++y)
        for (int x=0;x<LCD_COLS;++x)
            fputc(display.bits[y/8][x] & (1u<<(y%8)) ? 0 : 255,file);
    assert(fclose(file)==0);
}

static void page(enum Page view,int choice)
{
    ui.page=view;
    ui.choice=choice;
    draw_frame(&display,&pet,&ui);
    ++frames;
}

int main(int argc,char **argv)
{
    for(int cue=MOVE_TONE;cue<CUE_COUNT;++cue) {
        Sound s={(enum Cue)cue,0};unsigned count=0,hz;
        while((hz=sound_note(&s))) {assert(hz>=100 && hz<=2000 && ++count<=4);}
        assert(count && !sound_note(&s));
    }
    Sound fake={FLAG_SHORT,0},real={FLAG_LONG,0};
    assert(sound_note(&fake)==sound_note(&real) && !sound_note(&fake) && !sound_note(&real));
    assert(sound_duration(FLAG_SHORT)<sound_duration(FLAG_LONG));
    output=argc>1 ? argv[1] : NULL;
    pet_new(&pet,600); pet.character=V3_LEGACY; pet.generation=0;
    ui_start(&ui);
    page(TITLE,0); sample("title",0);
    for (unsigned i=1;i<=12;++i) { pet.setup=i; page(HOME,0); sample("setup",i); }
    pet.setup=0;
    for (unsigned stage=EGG;stage<=DEAD;++stage) {
        pet.stage=stage;
        for (int selection=-1;selection<ICON_COUNT;++selection) {
            ui.icon=selection;
            for (int i=0;i<48;++i) {
                ui.frame=i; pet.dark=i%2; pet.illness=i%3; pet.mess=i%5;
                pet.last=i%2 ? 1320 : 600;
                page(HOME,0);
            }
        }
    }
    pet.stage=CHILD; pet.dark=0; pet.last=600; pet.hunger=2; pet.happy=3;
    pet.points=9999; pet.minutes=52595999; pet.weight=99;
    pet.paused=1; page(HOME,0); sample("pause",0); pet.paused=0;
    for (int i=0;i<7;++i) { page(STATUS,i); sample("status",i); }
    for (int r=0;r<2;++r)
        for (int stage=EGG;stage<=DEAD;++stage) { pet.stage=stage; pet.route=r; page(STATUS,4); }
    pet.stage=ADULT;
    for (int i=0;i<2;++i) {
        page(FOOD,i); sample("food",i);
        page(TRAIN,i); sample("train",i);
        ui.snack_menu=i;
        for (int j=0;j<4+i;++j) { page(DISH,j); sample(i?"snacks":"meals",j); }
    }
    for (int i=0;i<4;++i) {
        pet.sold=0; page(SHOP,i); sample("shop",i);
        pet.sold=15; page(SHOP,i);
    }
    for (int i=0;i<3;++i) {
        page(ACTIV_MENU,i); sample("activity",i);
        page(ITEMS,i); sample("items",i);
        for (ui.frames=24;ui.frames>=1;--ui.frames) page(USING_ITEM,i);
        sample("using",i);
    }
    for (int i=0;i<ACTIVITIES;++i) {
        page(GAMES,i); sample("games",i);
        play_start(&ui.play,&pet,i);
        for (int t=0;t<40;++t) {
            ui.play.ticks=t;
            if ((i==GET || i==HEADING) && t>12) break;
            ui.play.input=t%16; ui.play.power=t%6; ui.play.answered=t%2;
            ui.play.lane=t%3; ui.play.target=t%3; ui.play.hazard=t%2;
            page(PLAYING,0);
            if (t==4) sample("playing",i);
        }
        ui.play.done=true; ui.play.score=100; page(PLAYING,0); sample("result",i);
    }
    const char *const notices[]={"N/A","EAT","NO","BOUGHT","OK","REST"};
    for (unsigned i=0;i<sizeof(notices)/sizeof(*notices);++i) {
        ui.message=notices[i]; page(NOTICE,0); sample("notice",i);
    }
    pet.stage=DEAD;
    const enum Page menus[]={OPTIONS,SETTINGS,INPUT_MODE};
    for (unsigned m=0;m<sizeof(menus)/sizeof(*menus);++m) {
        ui.page=menus[m];
        for (int choice=0;choice<ui_count(&ui,&pet);++choice) page(menus[m],choice);
    }
    /* Every symbol stays entirely inside the outline. Selection cannot alter
       its 36x36 raster, the pet region, or the other nine symbols. */
    static Display neutral;
    ui.icon=-1; ui.frame=0; pet.stage=ADULT; pet.dark=0; page(HOME,0);
    neutral=display;
    for (int i=0;i<ICON_COUNT;++i) {
        ui.icon=i; page(HOME,0);
        int x=24+60*(i%5), y=i<5 ? 2 : 202;
        int ink=0;
        for (int dy=0;dy<36;++dy) for (int dx=0;dx<36;++dx) {
            unsigned mask=1u<<((y+dy)%8);
            unsigned glyph=neutral.bits[(y+dy)/8][x+dx]&mask;
            assert((display.bits[(y+dy)/8][x+dx]&mask)==glyph);
            ink+=glyph!=0;
            if (i==IC_STATUS) assert(glyph==(neutral.bits[(y+dy)/8][x+35-dx]&mask));
        }
        assert(ink>=72 && ink<36*36);
        assert((display.bits[y/8][x-5]&(1u<<(y%8)))!=0);
        assert(!memcmp(display.bits[15],neutral.bits[15],7*LCD_COLS));
        sample("selected",i);
    }
    /* Draw every supported species through actual home, care and reveal layouts. */
    for (unsigned id=V3_EGG;id<=V3_WARUSOTCHI;++id) {
        pet.character=id;
        pet.stage=id==V3_EGG?EGG:v3_characters[id].life_class+1u;
        for (unsigned f=0;f<64;++f) {
            ui.frame=f;
            pet.egg_seconds=44+f%16;
            pet.happy=f%5; pet.hunger=f%5; pet.dark=f%2;
            pet.illness=f%3;
            pet.last=(f%2)?600:1320;
            page(HOME,0);
            ui.message="EAT"; page(NOTICE,0);
            ui.message="NO"; page(NOTICE,0);
            ui.message="OK"; page(NOTICE,0);
            if (pet.stage>=CHILD && f<20) {
                ui.growth_from=id-1; ui.growth_frames=f+1; page(HOME,0); ui.growth_frames=0;
            }
            if (f<11) page(STATUS,f);
            if (pet.stage==BABY) {
                pet.setup=REVEAL_BABY; page(HOME,0); pet.setup=0;
            }
        }
    }
    pet.stage=ADULT;pet.character=V3_MAMETCHI;pet.family.mate=V3_WARUSOTCHI;
    pet.family.count=1;pet.generation=9999;
    pet.family.records[0]=(Ancestor){0,V3_OJITCHI,V3_OTOKITCHI,0,MAX_CLOCK,1,2,0};
    for(unsigned detail=0;detail<3;++detail) {ui.detail=detail;page(LINEAGE,0);}
    for(unsigned frame=0;frame<=48;++frame) {
        ui.frame=frame;ui.frames=frame;
        page(WEDDING,0);page(OFFER,frame%2);page(DEPARTURE,0);
    }
    pet.shop.souvenirs=UINT32_MAX;
    for(int i=0;i<32;++i) page(SOUVENIRS,i);
    for(unsigned id=0;id<GOODS_COUNT;++id) {
        ui.item=id;ui.frames=24;ui.amount=500;page(USING_ITEM,0);
    }
    for(unsigned id=V3_TELETCHI_BOY;id<=V3_OYAJITCHI;++id) {
        pet.character=id;pet.stage=id<=V3_TELETCHI_GIRL?BABY:id<=V3_MOHITAMATCHI?CHILD:id<=V3_HASHITAMATCHI?TEEN:ADULT;
        pet.last=720;pet.illness=1;
        for(unsigned action=MEAL;action<=LIGHT;++action) {
            ui.care_scene=action+1;ui.message="Care";ui.item=GOODS_COUNT;
            for(unsigned beat=0;beat<12;++beat) {
                ui.frames=12-beat;ui.frame=beat;ui.detail=beat%2;page(NOTICE,0);
                if(id==V3_MAMETCHI && beat==5) sample("care",action);
            }
        }
        ui.care_scene=0;pet.illness=0;
        for(unsigned f=1;f<=32;++f) {
            ui.frames=f;ui.frame=f;
            for(unsigned k=0;k<GOODS_COUNT;++k) {
                if(id!=V3_MAMETCHI && k!=BALL) continue;
                ui.item=k;ui.amount=0;page(USING_ITEM,0);
                if(id==V3_MAMETCHI && f==6) sample("item",k);
            }
        }
    }
    pet.hunger=4;pet.happy=4;pet.call=QUIET;pet.mess=0;pet.dark=0;ui.icon=IC_STATUS;
    const unsigned forms[]={V3_TAMATCHI,V3_MAMETCHI,V3_MAMETCHI,V3_MAMETCHI,V3_MAMETCHI};
    const unsigned times[]={770,570,1110,1240,203};
    for(unsigned i=0;i<5;++i) {
        pet.character=forms[i];pet.stage=i==0?CHILD:ADULT;pet.last=times[i];
        pet.awake=37-pet.character;pet.illness=0;ui.frame=6;page(HOME,0);sample("daily",i);
    }
    pet.character=V3_HINOTAMATCHI;pet.stage=TEEN;ui.item=BALL;
    for (ui.frames=71;ui.frames>0;--ui.frames) {
        page(USING_ITEM,0);
        if(ui.frames%8==0) sample("ball",ui.frames);
    }
    const enum Page commerce[]={NOTEBOOK,CLERK,SHOP_CODE,PASSWORD,CLAIM,DONATION,DONATE_ASK,MEMORIAL};
    ui.amount=9999;pet.shop.donated=30000;
    for(unsigned i=0;i<sizeof commerce/sizeof *commerce;++i) {
        ui.page=commerce[i];page(commerce[i],0);
        for(int choice=0;choice<ui_count(&ui,&pet);++choice) page(commerce[i],choice);
    }
    for (unsigned id=V3_TELETCHI_BOY;id<V3_COUNT;++id) {
        ui.detail=0;page(JOURNAL,id-2);
        journal_meet(&pet,id);ui.detail=0;page(JOURNAL,id-2);
        ui.detail=1;page(JOURNAL,id-2);
    }
    page(PROGRESS,0);
    pet.stage=ADULT;pet.character=V3_HANATCHI;pet.last=civil_minute(2026,4,9,16,30);
    pet.family.phase=SINGLE;pet.shop.worn=0;pet.mess=0;pet.illness=0;pet.call=0;
    for (ui.frame=0;ui.frame<160;++ui.frame) page(HOME,0);
    sample("easter",0);
    /* Wall-clock event begins at the source GIF's first frame, independent
       of how long this plugin instance has been running. */
    ui.frame=73;ui.page=HOME;ui_tick(&ui,&pet);page(HOME,0);
    Display first_easter=display;
    ui.frame=ui.minute_frame+15;page(HOME,0);
    assert(!memcmp(&first_easter,&display,sizeof display));
    ui.frame=ui.minute_frame+16;page(HOME,0);
    assert(memcmp(&first_easter,&display,sizeof display));
    ui.item=PEN;ui.amount=0;
    for (pet.character=V3_MAMETCHI;pet.character<=V3_OYAJITCHI;++pet.character) {
        ui.frames=17;page(USING_ITEM,0);
        ui.frames=16;page(USING_ITEM,0);
        sample("drawing",pet.character);
    }
    pet.character=V3_MAMETCHI;
    pet.family.phase=WITH_BABY;pet.family.child_doses=2;
    pet.last=720;ui.growth_frames=0;ui.frame=0;page(HOME,0);
    ui.detail=1;ui.care_scene=MEDICINE+1;ui.message="Baby medicine";page(NOTICE,0);
    pet.family.phase=SINGLE;pet.dark=1;ui.frame=ui.minute_frame=100;
    pet.last=civil_minute(2026,12,24,22,30);
    page(HOME,0);sample("santa",0);Display arrival=display;
    ui.frame+=16;page(HOME,0);sample("santa",1);
    assert(memcmp(&display,&arrival,sizeof display));
    pet.last=civil_minute(2027,1,1,0,0);page(HOME,0);sample("new-year",0);
    pet.dark=0;pet.shop.souvenirs=0;pet.shop.visited=31;page(SOUVENIRS,9);sample("ticket-code",0);
    for (int c=POINT_BAG;c<=BIG_FISH;++c) {
        ui.caught=(enum Catch)c;ui.item=FISHING_POLE;ui.frames=4;
        page(USING_ITEM,0);sample("catch",c);
    }
    for (int type=BUMP;type<=SPRINT;type+=SPRINT-BUMP) {
        play_start(&ui.play,&pet,(enum Activity)type);
        for(int outcome=0;outcome<2;++outcome) {
            ui.play.round_won=outcome!=0;
            for (int t=1;t<=(type==BUMP?BUMP_CLASH_TICKS:SPRINT_FINISH_TICKS);++t) {
                ui.play.finish_ticks=t;page(PLAYING,0);
                if (t==1) sample(type==BUMP?"bump":"sprint",outcome);
            }
        }
    }
    pet.dark=0;pet.family.phase=SINGLE;ui.item=GOODS_COUNT;
    ui.detail=0;ui.care_scene=MEAL+1;ui.message="Meal";
    for (unsigned id=V3_TELETCHI_BOY;id<=V3_OYAJITCHI;++id) {
        pet.character=id;pet.stage=id<4?BABY:id<8?CHILD:id<18?TEEN:ADULT;
        ui.frame=0;page(NOTICE,0);sample("sheet-eat-a",id);
        ui.frame=4;page(NOTICE,0);sample("sheet-eat-b",id);
    }
    pet.last=720;pet.dark=0;pet.illness=2;pet.call=0;pet.mess=0;
    ui.growth_frames=0;
    for (int kind=0;kind<3;++kind) {
        pet.stage=kind?ADULT:BABY;pet.character=kind?V3_MAMETCHI:V3_TELETCHI_BOY;
        pet.minutes=kind?7000:15;pet.cavities=kind==2;
        page(HOME,0);sample("illness-icon",kind);
    }
    page(CLOCK,0); sample("clock",0);
    for (unsigned i=0;i<DISPLAY_TEST_PAGES;++i) {
        draw_display_test(&display,i,0); sample("diagnostic",i);
    }
    printf("PASS %d gameplay frames and %d diagnostic pages\n",frames,DISPLAY_TEST_PAGES);
}

