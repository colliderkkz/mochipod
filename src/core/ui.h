#ifndef MOCHI_UI_H
#define MOCHI_UI_H
#include "play.h"

enum Page { TITLE, HOME, STATUS, FOOD, TRAIN, ACTIV_MENU, SHOP, ITEMS, USING_ITEM,
            DISH, GAMES, PLAYING, CLOCK, NOTICE, OPTIONS, SETTINGS, INPUT_MODE,
            DISPLAY_TEST, EXIT_ASK, HATCH_ASK, REPLACE_ASK, NOTEBOOK, LINEAGE, OFFER, WEDDING, DEPARTURE, MEMORIAL, CLERK, SHOP_CODE, PASSWORD, CLAIM, SOUVENIRS, DONATION, DONATE_ASK, JOURNAL, PROGRESS };
enum Icon { IC_STATUS, IC_FEED, IC_TOILET, IC_GAME, IC_CONNECT, IC_LIGHT,
            IC_MEDICINE, IC_DISCIPLINE, IC_FRIENDS, IC_ATTENTION, ICON_COUNT };
extern const char *const icon_names[ICON_COUNT];
enum InputMode { NATIVE_IPOD, CLASSIC_V3 };
enum Command { CMD_NONE, CMD_PREV, CMD_NEXT, CMD_OK, CMD_BACK, CMD_EXIT,
               CMD_STATUS, CMD_CANCEL, CMD_LEFT, CMD_RIGHT };
#define DISPLAY_TEST_PAGES 4
typedef struct { enum Page page; int choice; } MenuFocus;
typedef struct {
    enum Page page;
    int icon, choice, frames;
    unsigned item, digits, knocks, knock_start, amount, detail;
    int code;
    enum Catch caught;
    unsigned care_scene; /* successful care enum + 1, zero for general notices */
    char entry[11];
    unsigned growth_from, growth_frames;
    bool snack_menu, refused_meal, exit_requested;
    enum InputMode mode;
    MenuFocus parents[10];
    unsigned depth, frame, minute_frame, animation_minute, offer_frame;
    const char *message;
    Play play;
} Ui;
bool ui_confirm_prompt(enum Page page);
void ui_start(Ui *u);
void ui_command(Ui *u,Pet *p,enum Command cmd);
void ui_growth(Ui *u,const Pet *p,unsigned previous);
void ui_tick(Ui *u,Pet *p);
int ui_count(const Ui *u,const Pet *p);
#endif
