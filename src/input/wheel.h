#ifndef MOCHI_WHEEL_H
#define MOCHI_WHEEL_H
#include "../core/ui.h"
enum Control { WHEEL_CW,WHEEL_CCW,CLICK_CENTER,CLICK_MENU,CLICK_PLAY,CLICK_LEFT,CLICK_RIGHT };
enum Phase { PRESSED,REPEATED,RELEASED };
typedef struct {
    uint32_t wheel_tick;
    bool scrolled,menu_down,menu_long,play_down;
    unsigned held;
} WheelInput;
enum Command wheel_command(WheelInput *input,const Ui *ui,enum Control key,enum Phase phase,
                           uint32_t tick,unsigned hz);
#endif
