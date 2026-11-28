#include "wheel.h"

enum Command wheel_command(WheelInput *in,const Ui *u,enum Control key,enum Phase phase,
                           uint32_t tick,unsigned hz)
{
    bool game=u->page==PLAYING && !u->play.done;
    if (key==WHEEL_CW || key==WHEEL_CCW) {
        if (phase==RELEASED || (u->mode==CLASSIC_V3 && u->page!=DISPLAY_TEST)) return CMD_NONE;
        if (game && u->play.type!=GET && u->play.type!=HEADING) return CMD_NONE;
        /* The driver already thresholds physical displacement. A queued event
           moves one item; coalesced velocity never multiplies the step. */
        if (in->scrolled && tick-in->wheel_tick<(hz+9)/10) return CMD_NONE;
        in->scrolled=true; in->wheel_tick=tick;
        if (game) return key==WHEEL_CW ? CMD_RIGHT : CMD_LEFT;
        return key==WHEEL_CW ? CMD_NEXT : CMD_PREV;
    }
    if (key==CLICK_MENU) {
        if (phase==PRESSED) { in->menu_down=true; in->menu_long=false; }
        if (phase==REPEATED && in->menu_down && !in->menu_long && !ui_confirm_prompt(u->page)) {
            in->menu_long=true; return CMD_EXIT;
        }
        if (phase==RELEASED) {
            bool short_press=in->menu_down && !in->menu_long;
            in->menu_down=false;
            if (short_press) return CMD_BACK;
        }
        return CMD_NONE;
    }
    if (key==CLICK_PLAY) {
        if (phase==PRESSED) in->play_down=true;
        if (phase==REPEATED) in->play_down=false;
        if (phase==RELEASED && in->play_down) { in->play_down=false; return CMD_STATUS; }
        return CMD_NONE;
    }
    unsigned mask=1u<<key;
    if (phase==RELEASED) { in->held&=~mask; return CMD_NONE; }
    if (phase==REPEATED || (in->held&mask)) return CMD_NONE;
    in->held|=mask;
    if (u->page==SHOP_CODE && u->mode==CLASSIC_V3) {
        return key==CLICK_CENTER?CMD_OK:key==CLICK_LEFT?CMD_LEFT:CMD_RIGHT;
    }
    if (key==CLICK_CENTER) return CMD_OK;
    if (game) return key==CLICK_LEFT ? CMD_LEFT : CMD_RIGHT;
    if (u->mode==CLASSIC_V3) return key==CLICK_LEFT ? CMD_NEXT : CMD_CANCEL;
    return CMD_NONE;
}

