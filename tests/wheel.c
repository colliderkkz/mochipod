#include "../src/input/wheel.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Ui u;
static Pet p;
static WheelInput wheel;
static uint32_t tick;

static enum Command send(enum Control key,enum Phase phase)
{
    enum Command command=wheel_command(&wheel,&u,key,phase,tick,100);
    ui_command(&u,&p,command);
    return command;
}
static void click(enum Control key)
{
    tick+=15;
    send(key,PRESSED); send(key,RELEASED);
}
static void reset(void)
{
    pet_new(&p,600); p.character=V3_LEGACY; p.generation=0; p.setup=0; p.stage=ADULT;
    ui_start(&u); wheel=(WheelInput){0}; tick=0;
    click(CLICK_CENTER);
}
static void wheel_test(void)
{
    reset();
    assert(u.icon==IC_STATUS);
    assert(send(WHEEL_CW,PRESSED)==CMD_NEXT && u.icon==1);
    for (int i=0;i<9;++i) { ++tick; assert(send(WHEEL_CW,REPEATED)==CMD_NONE); }
    ++tick; assert(send(WHEEL_CW,REPEATED)==CMD_NEXT && u.icon==2);
    for (int i=0;i<10;++i) click(WHEEL_CCW);
    assert(u.icon==2);
    click(WHEEL_CCW); click(WHEEL_CCW); click(WHEEL_CCW); assert(u.icon==IC_ATTENTION);
    tick=UINT32_MAX-3; click(WHEEL_CW); assert(u.icon==IC_STATUS);
    send(CLICK_CENTER,PRESSED); assert(u.page==STATUS);
    send(CLICK_CENTER,REPEATED); send(CLICK_CENTER,PRESSED); assert(u.choice==0);
    send(CLICK_CENTER,RELEASED);
    click(CLICK_MENU); assert(u.page==HOME && u.icon==IC_STATUS);
    send(CLICK_MENU,PRESSED); send(CLICK_MENU,REPEATED); send(CLICK_MENU,REPEATED);
    send(CLICK_MENU,RELEASED); assert(u.page==EXIT_ASK && !u.exit_requested && u.depth==1);
    click(CLICK_CENTER); assert(u.page==HOME);
    click(CLICK_PLAY); assert(u.page==STATUS);
    click(CLICK_MENU); assert(u.page==HOME);
}
static void menu_test(void)
{
    reset(); click(WHEEL_CW); click(CLICK_CENTER);
    click(WHEEL_CW); click(CLICK_CENTER); assert(u.page==DISH && u.snack_menu);
    click(WHEEL_CCW); assert(u.choice==3);
    click(CLICK_MENU); assert(u.page==FOOD && u.choice==1);
    click(CLICK_MENU); assert(u.page==HOME && u.icon==IC_FEED);
    click(CLICK_MENU); click(WHEEL_CW); click(CLICK_CENTER); assert(u.page==SETTINGS);
    Pet saved=p;
    click(CLICK_CENTER); click(WHEEL_CW); click(CLICK_CENTER);
    assert(u.mode==CLASSIC_V3 && !memcmp(&p,&saved,sizeof p));
    click(CLICK_LEFT); click(CLICK_CENTER); assert(u.page==DISPLAY_TEST);
    click(WHEEL_CCW); assert(u.choice==3);
    click(CLICK_CENTER); assert(u.choice==0);
    click(CLICK_MENU); assert(u.page==HOME && u.icon==IC_FEED);
    click(CLICK_RIGHT); assert(u.icon==-1);
    click(CLICK_LEFT); assert(u.icon==IC_STATUS);
    reset(); p.setup=1;
    click(WHEEL_CCW); assert(p.birthday_month==12);
    click(CLICK_CENTER); click(WHEEL_CCW); assert(p.birthday_day==31);
    click(CLICK_MENU); click(WHEEL_CW); click(WHEEL_CW);
    assert(p.birthday_month==2 && p.birthday_day==29 && pet_valid(&p));
    click(CLICK_CENTER); click(CLICK_CENTER); click(WHEEL_CCW);
    assert(name_letter(p.user_name,0)=='Z'); click(WHEEL_CW); assert(p.user_name==0);
}
static void icon_test(void)
{
    const enum Page pages[]={STATUS,FOOD,NOTICE,ACTIV_MENU,NOTICE,NOTICE,NOTICE,TRAIN,NOTEBOOK,STATUS};
    for (int i=0;i<ICON_COUNT;++i) {
        reset(); p.mess=1; p.illness=1;
        for (int n=0;n<i;++n) click(WHEEL_CW);
        click(CLICK_CENTER);
        assert(u.page==pages[i]);
        if (i==IC_LIGHT) assert(p.dark);
        if (i==IC_TOILET) assert(!p.mess);
        if (i==IC_MEDICINE) assert(!p.illness);
        if (i==IC_CONNECT) assert(!strcmp(u.message,"Infrared only"));
    }
}
static void game_test(void)
{
    for (int mode=0;mode<2;++mode) {
        for (int type=0;type<ACTIVITIES;++type) {
            reset(); u.page=PLAYING; u.mode=mode;
            play_start(&u.play,&p,type);
            for (int key=0;key<3;++key) {
                Play expected=u.play; Pet after=p;
                play_key(&expected,&after,(enum Key)key);
                click(((const enum Control[]){CLICK_LEFT,CLICK_CENTER,CLICK_RIGHT})[key]);
                assert(!memcmp(&u.play,&expected,sizeof expected));
                assert(!memcmp(&p,&after,sizeof p));
                play_start(&u.play,&p,type);
            }
            int old=u.play.lane;
            click(WHEEL_CW);
            assert(u.play.lane==old+(mode==NATIVE_IPOD && (type==GET || type==HEADING)));
        }
    }
}
int main(void)
{
    reset();unsigned accepted=0;
    for(unsigned i=0;i<20000;++i) {tick=i/100;accepted+=send(WHEEL_CW,REPEATED)==CMD_NEXT;}
    assert(accepted==20 && u.icon==IC_STATUS);
    assert(send(CLICK_CENTER,PRESSED)==CMD_OK && u.page==STATUS);
    for(unsigned i=0;i<1000;++i) assert(send(CLICK_CENTER,REPEATED)==CMD_NONE);
    assert(u.page==STATUS && u.choice==0);
    wheel_test(); menu_test(); icon_test(); game_test();
    puts("PASS wheel filtering, holds, menus, settings, icon actions and six game mappings");
    return 0;
}

