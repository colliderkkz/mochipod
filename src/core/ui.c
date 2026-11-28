#include "ui.h"
#include "growth.h"

const char *const icon_names[ICON_COUNT]={
    "STATUS","FEED","TOILET","GAME","CONNECT","LIGHT","MEDICINE","DISCIPLINE","FRIENDS","ATTENTION"
};

static void home(Ui *u)
{
    u->page=HOME; u->choice=0; u->depth=0;
    if (u->icon<0 && u->mode==NATIVE_IPOD) u->icon=IC_STATUS;
}
void ui_start(Ui *u) { *u=(Ui){.page=TITLE,.icon=-1}; }

static void enter(Ui *u,enum Page page)
{
    if (u->depth>=sizeof(u->parents)/sizeof(u->parents[0])) return;
    u->parents[u->depth++]=(MenuFocus){u->page,u->choice};
    u->page=page;
    if (page==OFFER) u->offer_frame=u->frame;
    u->choice=0;
}
static void back(Ui *u)
{
    if (!u->depth) { home(u); return; }
    MenuFocus parent=u->parents[--u->depth];
    u->page=parent.page;
    u->choice=parent.choice;
}
static void notice(Ui *u,const char *message)
{
    enter(u,NOTICE);u->care_scene=0;u->refused_meal=false; u->message=message; u->frames=12;
}
static void care_denied(Ui *u,const Pet *p,enum Care action)
{
    bool refusal=action==MEAL && p->character!=V3_LEGACY && p->call==NAUGHTY &&
                 !pet_sleeping(p) && !p->illness;
    notice(u,refusal?"Refused":pet_sleeping(p)?"Asleep":p->illness?"Sick":"No need");
    u->refused_meal=refusal;
}
static void care(Ui *u,Pet *p,enum Care action)
{
    static const char *const labels[]={"Meal","Snack","Clean","Medicine","Praise","Time out","Lights"};
    bool toilet=action==FLUSH && pet_needs_toilet(p);
    bool baby=action==MEDICINE && p->family.phase==WITH_BABY && p->family.child_doses;
    if (!pet_care(p,action)) {care_denied(u,p,action);return;}
    notice(u,baby?"Baby medicine":toilet?"Toilet":labels[action]);
    u->care_scene=action+1;u->item=GOODS_COUNT;u->detail=toilet || baby;
}

int ui_count(const Ui *u,const Pet *p)
{
    switch (u->page) {
    case NOTEBOOK: return 6;
    case JOURNAL: return V3_COUNT-V3_TELETCHI_BOY;
    case MEMORIAL:return p->memorial_count;
    case SOUVENIRS:return 32;
    case SHOP_CODE:return 3;
    case PASSWORD: case DONATION:return 10;
    case LINEAGE: return p->family.count;
    case HOME: return ICON_COUNT;
    case STATUS: return p->character==V3_LEGACY ? 7 : 11;
    case FOOD: case TRAIN: case INPUT_MODE: return 2;
    case ACTIV_MENU:return 5;
    case ITEMS:return owned_count(p,false,false);
    case OPTIONS: return 5;
    case SETTINGS: return 5;
    case DISPLAY_TEST: return DISPLAY_TEST_PAGES;
    case GAMES: return play_available(p);
    case SHOP: return 4;
    case DISH: return 4+owned_count(p,true,u->snack_menu);
    default: return 0;
    }
}
static void move(Ui *u,const Pet *p,int delta)
{
    int count=ui_count(u,p);
    if (!count) return;
    int *selected=u->page==HOME ? &u->icon : &u->choice;
    if (*selected<0) *selected=delta>0 ? 0 : count-1;
    else *selected=(*selected+count+delta)%count;
}
static void activate_icon(Ui *u,Pet *p)
{
    if (p->stage==DEAD && u->icon<0) { enter(u,HATCH_ASK); return; }
    switch (u->icon) {
    case IC_STATUS: case IC_ATTENTION: enter(u,STATUS); break;
    case IC_FEED: enter(u,FOOD); break;
    case IC_TOILET: care(u,p,FLUSH); break;
    case IC_GAME:
        if (play_available(p) && !pet_sleeping(p)) enter(u,ACTIV_MENU);
        else notice(u,"REST");
        break;
    case IC_LIGHT: care(u,p,LIGHT); break;
    case IC_MEDICINE: care(u,p,MEDICINE); break;
    case IC_DISCIPLINE: enter(u,TRAIN); break;
    case IC_CONNECT: notice(u,"Infrared only"); break;
    case IC_FRIENDS: enter(u,NOTEBOOK); break;
    default: enter(u,CLOCK); break;
    }
}

static void confirm(Ui *u,Pet *p)
{
    int choice=u->choice;
    switch (u->page) {
    case NOTEBOOK:
        if (!choice) { enter(u,LINEAGE);u->detail=0; }
        else if (choice==1) enter(u,MEMORIAL);
        else if (choice==2) { enter(u,DONATION);u->amount=0;u->digits=0; }
        else if (choice==4) {enter(u,JOURNAL);u->detail=0;}
        else if (choice==5) enter(u,PROGRESS);
        else if (family_elder_offer(p)) {journal_meet(p,p->family.mate);enter(u,OFFER);}
        else notice(u,"Elders only");
        break;
    case LINEAGE:u->detail=(u->detail+1)%3;break;
    case MEMORIAL: case SOUVENIRS: move(u,p,1); break;
    case OFFER:
        if (family_answer(p,choice!=0) && choice) {
            home(u);enter(u,WEDDING);u->frames=48;
        } else home(u);
        break;
    case WEDDING: home(u);break;
    case JOURNAL: u->detail^=1;break;
    case PROGRESS: back(u);break;
    case DEPARTURE: if (family_takeover(p)) home(u); break;
    case TITLE: home(u); break;
    case HOME: activate_icon(u,p); break;
    case OPTIONS:
        if (choice==0) back(u);
        else enter(u,((const enum Page[]){HOME,SETTINGS,CLOCK,EXIT_ASK,HATCH_ASK})[choice]);
        break;
    case SETTINGS:
        if (choice==0) { enter(u,INPUT_MODE); u->choice=u->mode; }
        else if (choice==1) enter(u,DISPLAY_TEST);
        else if (choice==2) p->muted^=1;
        else if (choice==4) p->sound_level=(p->sound_level+1)%3;
        else { p->paused^=1; home(u); }
        break;
    case INPUT_MODE: u->mode=(enum InputMode)choice; back(u); break;
    case EXIT_ASK: if (choice) u->exit_requested=true; else back(u); break;
    case HATCH_ASK:
        if (choice) enter(u,REPLACE_ASK);
        else back(u);
        break;
    case REPLACE_ASK:
        if (choice) { pet_restart(p); u->icon=-1; home(u); }
        else back(u);
        break;
    case ACTIV_MENU:
        enter(u,((const enum Page[]){GAMES,ITEMS,CLERK,PASSWORD,SOUVENIRS})[choice]);
        u->knocks=0;u->digits=0;u->entry[0]=0;
        break;
    case CLERK: enter(u,SHOP);break;
    case SHOP: {
        unsigned id=shelf_item(p,choice);
        if (buy_goods(p,id)) notice(u,"Bought");
        else notice(u,p->sold&(1u<<choice)?"Sold out":pet_sleeping(p)?"Asleep":
                    p->points<shelf_price(p,id)?"Need points":"Bag full");
        break;
    }
    case CLAIM:
        back(u);
        if (choice) notice(u,redeem_code(p,u->code)?"Received":"NO");
        break;
    case SHOP_CODE: case PASSWORD: {
        bool numeric=u->page==PASSWORD;
        u->entry[u->digits++]=(numeric?'0':'A')+choice;
        u->entry[u->digits]=0;u->choice=0;
        if (u->digits==(numeric?10u:8u)) {
            u->code=code_item(u->entry,numeric);
            if (numeric && u->code<0) u->code=town_password(p,u->entry);
            u->digits=0;u->entry[0]=0;
            if (u->code<0) notice(u,"Wrong code");
            else enter(u,CLAIM);
        }
        break;
    }
    case DONATION:
        u->amount=u->amount*10+choice;u->choice=0;
        if (++u->digits==4) { u->digits=0;enter(u,DONATE_ASK); }
        break;
    case DONATE_ASK: {
        bool ok=choice && donate(p,u->amount);
        back(u);u->amount=0;
        if (choice) {
            char code[11];
            bool pending=town_receipt(p,14,code) || town_receipt(p,15,code) || town_receipt(p,16,code);
            notice(u,pending?"Code in Souvenirs":ok?"Thank you":"NO");
        }
        break;
    }
    case ITEMS: {
        u->item=owned_item(p,choice,false,false);
        if (u->item==LOVE_POTION) {notice(u,"Infrared only");break;}
        unsigned old_points=p->points;
        if (use_item_result(p,u->item,&u->caught)) {
            u->amount=p->points-old_points;
            if (u->choice>=ui_count(u,p)) u->choice=0;
            enter(u,USING_ITEM);u->frames=u->item==BALL && p->character==V3_HINOTAMATCHI?71:32;
        } else {const char *why=item_gate(p,u->item);notice(u,why?why:"No item");}
        break;
    }
    case DISH:
        if (choice>=4) {
            unsigned id=owned_item(p,choice-4,true,u->snack_menu);
            bool ok=use_item(p,id);
            if (u->choice>=ui_count(u,p)) u->choice=0;
            enum Care action=u->snack_menu?SNACK:MEAL;
            if (!ok) {care_denied(u,p,action);break;}
            notice(u,goods_names[id]);
            u->care_scene=action+1;u->item=id;u->detail=0;
        } else care(u,p,u->snack_menu?SNACK:MEAL);
        break;
    case FOOD: u->snack_menu=choice!=0; enter(u,DISH); break;
    case TRAIN: care(u,p,choice?PRAISE:TIME_OUT); break;
    case GAMES:
        if (choice>=play_available(p) || pet_sleeping(p)) { notice(u,"REST"); break; }
        if (p->illness) {notice(u,"Medicine first");break;}
        if (p->weight<play_min_weight(p)+3) { notice(u,"Feed first");break; }
        play_start(&u->play,p,(enum Activity)choice); enter(u,PLAYING); break;
    case DISPLAY_TEST: case STATUS: move(u,p,1); break;
    case NOTICE: case USING_ITEM: case CLOCK: case PLAYING: back(u); break;
    }
}

bool ui_confirm_prompt(enum Page page)
{
    return page==EXIT_ASK || page==HATCH_ASK || page==REPLACE_ASK ||
           page==DONATE_ASK || page==CLAIM || page==OFFER;
}

void ui_command(Ui *u,Pet *p,enum Command cmd)
{
    if (cmd==CMD_NONE) return;
    if (u->page==CLERK && cmd==CMD_NEXT) {
        /* Manual: attract the clerk, then three quick A presses.
           Two seconds is the iPod input window, not a measured toy timing. */
        if (!u->knocks || u->frame-u->knock_start>=16) {
            u->knocks=1;
            u->knock_start=u->frame;
        } else if (++u->knocks==4) {
            u->knocks=0;u->digits=0;u->entry[0]=0;enter(u,SHOP_CODE);
        }
        return;
    }
    if (u->page==CLERK) u->knocks=0;
    if (u->page==SHOP_CODE && u->mode==CLASSIC_V3 &&
        (cmd==CMD_LEFT || cmd==CMD_OK || cmd==CMD_RIGHT)) {
        u->choice=cmd==CMD_LEFT?0:cmd==CMD_OK?1:2;confirm(u,p);return;
    }
    if (ui_confirm_prompt(u->page)) {
        /* Center answers yes and Menu no; the wheel does not answer. */
        if (cmd==CMD_OK || cmd==CMD_BACK || cmd==CMD_CANCEL) {
            u->choice=cmd==CMD_OK;
            confirm(u,p);
        }
        return;
    }
    if (cmd==CMD_EXIT) { enter(u,EXIT_ASK); return; }
    if (p->setup && u->page==HOME) {
        if (cmd==CMD_OK) setup_key(p,KEY_B);
        if (cmd==CMD_NEXT || cmd==CMD_PREV) setup_adjust(p,cmd==CMD_NEXT?1:-1);
        if (cmd==CMD_BACK || cmd==CMD_CANCEL) {
            if (p->setup==1 || p->setup==8 || p->setup==REVEAL_BABY) enter(u,OPTIONS);
            else --p->setup;
        }
        return;
    }
    if (p->character==V3_EGG && u->page==HOME && cmd!=CMD_BACK && cmd!=CMD_CANCEL) return;
    if (cmd==CMD_CANCEL && u->page==HOME) { u->icon=-1; return; }
    if (cmd==CMD_BACK || cmd==CMD_CANCEL) {
        if (u->page==DISPLAY_TEST) { home(u); return; }
        if (u->page==HOME) enter(u,OPTIONS);
        else back(u);
        return;
    }
    if (cmd==CMD_STATUS) {
        if (u->page==STATUS) back(u);
        else if (u->page==HOME || u->page==FOOD || u->page==TRAIN || u->page==ACTIV_MENU ||
                 u->page==DISH || u->page==GAMES || u->page==ITEMS || u->page==SHOP)
            enter(u,STATUS);
        return;
    }
    if (p->paused && u->page==HOME) {
        if (cmd==CMD_OK) enter(u,OPTIONS);
        return;
    }
    if (u->page==PLAYING && !u->play.done) {
        if (cmd==CMD_LEFT || cmd==CMD_RIGHT || cmd==CMD_OK)
            play_key(&u->play,p,cmd==CMD_OK?KEY_B:cmd==CMD_LEFT?KEY_A:KEY_C);
        return;
    }
    if (cmd==CMD_OK && u->page==HOME && u->growth_frames) {u->growth_frames=0;return;}
    if (cmd==CMD_NEXT || cmd==CMD_PREV) move(u,p,cmd==CMD_NEXT?1:-1);
    if (cmd==CMD_OK) confirm(u,p);
}
void ui_growth(Ui *u,const Pet *p,unsigned previous)
{
    u->growth_frames=0;
    if (p->character!=previous && p->character!=V3_LEGACY &&
        p->stage>=CHILD && p->stage<=ADULT) {
        unsigned from=history_at(p,(enum Stage)(p->stage-1));
        u->growth_from=from ? from : previous;
        u->growth_frames=20;
    }
}

void ui_tick(Ui *u,Pet *p)
{
    ++u->frame;
    if (u->animation_minute!=p->last || u->frame==1) {
        u->animation_minute=p->last;
        u->minute_frame=u->frame;
    }
    if (u->growth_frames && u->page==HOME) --u->growth_frames;
    if (u->page==TITLE && u->frame>=12) home(u);
    if (u->page==OFFER && (p->family.phase!=VISIT || p->stage==DEAD)) home(u);
    if (u->page==WEDDING && --u->frames<=0) home(u);
    if (u->page==HOME && !p->paused && p->stage==ADULT) {
        if (p->family.phase==VISIT) {journal_meet(p,p->family.mate);if (!is_senior(p)) journal_meet(p,V3_OTOKITCHI);enter(u,OFFER);}
        if (p->family.phase==HANDOVER) enter(u,DEPARTURE);
    }
    if ((u->page==NOTICE || u->page==USING_ITEM) && --u->frames<=0) back(u);
    if (u->page==GAMES && u->choice>=play_available(p)) back(u);
    if (u->page==PLAYING) {
        if (p->stage==DEAD || pet_sleeping(p) || p->illness) home(u);
        else play_tick(&u->play,p);
    }
}

