#include "../src/input/wheel.h"
#include "../src/render/lcd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Ui u;
static Pet p;
static WheelInput wheel;
static uint32_t tick;
static Display display;

static const struct Prompt {
    enum Page page, parent, accepted;
} prompts[]={
    {EXIT_ASK,OPTIONS,EXIT_ASK}, {HATCH_ASK,OPTIONS,REPLACE_ASK},
    {REPLACE_ASK,HATCH_ASK,HOME}, {DONATE_ASK,DONATION,NOTICE},
    {CLAIM,PASSWORD,NOTICE}, {OFFER,HOME,WEDDING},
};

static void open_prompt(const struct Prompt *prompt,enum InputMode mode)
{
    pet_new(&p,600);p.character=V3_LEGACY;p.generation=0;p.setup=0;p.stage=ADULT;
    p.points=300;p.family.phase=VISIT;p.family.mate=V3_WARUSOTCHI;
    ui_start(&u);u.mode=mode;u.icon=IC_STATUS;u.amount=5;u.code=0;
    u.depth=1;u.parents[0]=(MenuFocus){prompt->parent,0};
    u.page=prompt->page;
    wheel=(WheelInput){0};tick=0;
}

static void send(enum Control key,enum Phase phase)
{
    ui_command(&u,&p,wheel_command(&wheel,&u,key,phase,tick,100));
}

static void click(enum Control key)
{
    tick+=15;send(key,PRESSED);send(key,RELEASED);
}

static void check_confirm(const struct Prompt *prompt,enum InputMode mode)
{
    open_prompt(prompt,mode);
    Ui before=u;Pet original=p;
    const enum Control ignored[]={WHEEL_CW,WHEEL_CCW,CLICK_LEFT,CLICK_PLAY,CLICK_RIGHT};
    unsigned count=mode==NATIVE_IPOD?5:4;
    for (unsigned i=0;i<count;++i) {
        click(ignored[i]);
        assert(!memcmp(&u,&before,sizeof u) && !memcmp(&p,&original,sizeof p));
    }

    click(CLICK_CENTER);
    assert(u.page==prompt->accepted && u.exit_requested==(prompt->page==EXIT_ASK));
    switch (prompt->page) {
    case HATCH_ASK: assert(!memcmp(&p,&original,sizeof p));break;
    case REPLACE_ASK: assert(p.stage==EGG);break;
    case DONATE_ASK: assert(u.amount==0 && p.points==295 && p.shop.donated==5);break;
    case CLAIM: assert(p.shop.secrets!=original.shop.secrets);break;
    case OFFER: assert(p.family.phase==WITH_BABY);break;
    default: break;
    }
}

static void check_cancel(const struct Prompt *prompt,enum InputMode mode,enum Control key,bool hold)
{
    open_prompt(prompt,mode);
    Pet original=p;
    send(key,PRESSED);
    if (hold) {
        tick+=100;send(key,REPEATED);send(key,REPEATED);
        assert(u.page==prompt->page);
    }
    send(key,RELEASED);
    assert(u.page==prompt->parent && !u.exit_requested);
    if (prompt->page==OFFER) assert(p.family.phase==SINGLE);
    else assert(!memcmp(&p,&original,sizeof p));
    if (prompt->page==DONATE_ASK) assert(!u.amount);
}

static void check_layout(const struct Prompt *prompt)
{
    open_prompt(prompt,NATIVE_IPOD);
    assert(!ui_count(&u,&p));
    if (prompt->page==DONATE_ASK) u.amount=9999;
    if (prompt->page==CLAIM) {
        for (u.code=0;u.code<9+2*GOODS_COUNT+32;++u.code) draw_frame(&display,&p,&u);
    } else if (prompt->page==OFFER) {
        for (u.frame=0;u.frame<=48;++u.frame) draw_frame(&display,&p,&u);
    } else draw_frame(&display,&p,&u);
}

int main(void)
{
    for (unsigned i=0;i<sizeof prompts/sizeof *prompts;++i) {
        const struct Prompt *prompt=&prompts[i];
        for (int mode=NATIVE_IPOD;mode<=CLASSIC_V3;++mode) {
            check_confirm(prompt,mode);
            check_cancel(prompt,mode,CLICK_MENU,false);
            check_cancel(prompt,mode,CLICK_MENU,true);
            if (mode==CLASSIC_V3) check_cancel(prompt,mode,CLICK_RIGHT,false);
        }
        check_layout(prompt);
    }
    puts("PASS six yes/no prompts, both control modes and layout");
    return 0;
}
