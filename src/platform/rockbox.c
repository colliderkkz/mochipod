#include "plugin.h"
#include "../core/rtc_guard.h"
#include "../core/sound.h"
#include "../render/lcd.h"
#include "../input/wheel.h"
#include "../storage/save.h"

static const char *const legacy_paths[]={PLUGIN_GAMES_DATA_DIR "/mochipod-v3.a",PLUGIN_GAMES_DATA_DIR "/mochipod-v3.b"};
static const char *const hatching_paths[]={PLUGIN_GAMES_DATA_DIR "/mochipod-v6.a",PLUGIN_GAMES_DATA_DIR "/mochipod-v6.b"};
static const char *const growth_paths[]={PLUGIN_GAMES_DATA_DIR "/mochipod-v7.a",PLUGIN_GAMES_DATA_DIR "/mochipod-v7.b"};
static const char *const clock_paths[]={PLUGIN_GAMES_DATA_DIR "/mochipod-v8.a",PLUGIN_GAMES_DATA_DIR "/mochipod-v8.b"};
static const char *const family_paths[]={PLUGIN_GAMES_DATA_DIR "/mochipod-v9.a",PLUGIN_GAMES_DATA_DIR "/mochipod-v9.b"};
static const char *const paths[]={PLUGIN_GAMES_DATA_DIR "/mochipod-v12.a",PLUGIN_GAMES_DATA_DIR "/mochipod-v12.b"};
static const char *const journal_paths[]={PLUGIN_GAMES_DATA_DIR "/mochipod-v11.a",PLUGIN_GAMES_DATA_DIR "/mochipod-v11.b"};
static const char *const daily_paths[]={PLUGIN_GAMES_DATA_DIR "/mochipod-v10.a",PLUGIN_GAMES_DATA_DIR "/mochipod-v10.b"};
#define STAGING_PATH PLUGIN_GAMES_DATA_DIR "/mochipod-v12.tmp"
static Pet pet;
static int active_slot;
static uint32_t serial;
static unsigned verified_slots;
static Pet committed;

static uint32_t current_minute(unsigned *second)
{
    const struct tm *t=rb->get_time();
    *second=t->tm_sec;
    return civil_minute(t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min);
}

static bool load_slot(const char *path,Pet *out,uint32_t *seq)
{
    static uint8_t data[RECORD_BYTES];
    int fd=rb->open(path,O_RDONLY);
    if (fd<0) return false;
    off_t size=rb->filesize(fd);
    bool ok=(size==80 || size==92 || size==124 || size==152 || size==164 || size==180 || size==200 || size==204 || size==V9_BYTES || size==V10_BYTES || size==V11_BYTES || size==RECORD_BYTES) && rb->read(fd,data,size)==size;
    rb->close(fd);
    return ok && save_unpack(data,size,out,seq);
}

static bool commit_pet(const Pet *candidate)
{
    static uint8_t data[RECORD_BYTES];
    if (verified_slots==3 && !rb->memcmp(&committed,candidate,sizeof committed)) return true;
    int next=active_slot==0 ? 1 : 0;
    save_pack(data,candidate,serial+1);
    int fd=rb->open(STAGING_PATH,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd<0) return false;
    bool ok=rb->write(fd,data,sizeof(data))==sizeof(data);
    if (rb->close(fd)<0 || !ok) return false;
    static Pet verified;
    uint32_t seq;
    if (!load_slot(STAGING_PATH,&verified,&seq) || seq!=serial+1 ||
        rb->memcmp(candidate,&verified,sizeof verified) ||
        rb->rename(STAGING_PATH,paths[next])<0) return false;
    committed=*candidate;
    verified_slots|=1u<<next;
    active_slot=next;
    ++serial;
    return true;
}
static bool persist(void) { return commit_pet(&pet); }
#if defined(ASTRAPOD_PLUGIN_H) && defined(ASTRAPOD_ROCK_TRAY_H)
#include "tray.inc"
#else
#define tray_sync(p,on_exit) ((void)0)
#define tray_after_input(p) ((void)0)
#define tray_after_tick(p,event) ((void)0)
#endif

static void on_system_exit(void *ignored)
{
    (void)ignored;
    if (!persist()) rb->splash(HZ,"MochiPod: save failed");
    else tray_sync(&pet,true);
}

#if LCD_WIDTH != LCD_COLS || LCD_HEIGHT != LCD_ROWS
#error MochiPod display layout requires the 320x240 ipod6g LCD
#endif

static bool invalidate_lcd=true;
static void paint(const Ui *ui)
{
    /* 9600 bytes: keep this off the Rockbox plugin stack. */
    static Display display;
    draw_frame(&display,&pet,ui);
    static Display previous;
    int first=LCD_ROWS/8,last=-1;
    for (int row=0;row<LCD_ROWS/8;++row) {
        if (invalidate_lcd || rb->memcmp(previous.bits[row],display.bits[row],LCD_COLS)) {
            if (row<first) first=row;
            last=row;
        }
    }
    invalidate_lcd=false;
    if (last<first) return;
    int rows=last-first+1;
    rb->memcpy(previous.bits[first],display.bits[first],rows*LCD_COLS);
    rb->lcd_mono_bitmap(display.bits[first],0,8*first,LCD_COLS,8*rows);
    rb->lcd_update_rect(0,8*first,LCD_COLS,8*rows);
}

/* The presence of this zero-byte preference selects Classic. Older plugins
   ignore it; a mode switch never rewrites a pet save. */
#define CLASSIC_PREF PLUGIN_GAMES_DATA_DIR "/mochipod-classic.cfg"
static bool store_mode(enum InputMode mode)
{
    if (mode==NATIVE_IPOD) return rb->remove(CLASSIC_PREF)==0;
    int fd=rb->open(CLASSIC_PREF,O_WRONLY|O_CREAT,0666);
    return fd>=0 && rb->close(fd)==0;
}

static bool control_event(int button,enum Control *key)
{
    switch (button & ~(BUTTON_REL|BUTTON_REPEAT)) {
    case BUTTON_SCROLL_FWD: *key=WHEEL_CW; break;
    case BUTTON_SCROLL_BACK: *key=WHEEL_CCW; break;
    case BUTTON_LEFT: *key=CLICK_LEFT; break;
    case BUTTON_RIGHT: *key=CLICK_RIGHT; break;
    case BUTTON_SELECT: *key=CLICK_CENTER; break;
    case BUTTON_MENU: *key=CLICK_MENU; break;
    case BUTTON_PLAY: *key=CLICK_PLAY; break;
    default: return false;
    }
    return true;
}

/* Blocking review keeps game input, autosave and RTC simulation suspended. */
static void stamp_line(int y,const char *label,uint32_t minute)
{
    if (minute>MAX_CLOCK) { rb->lcd_putsxyf(8,y,"%s invalid",label); return; }
    int year=2000,month=1;
    unsigned day=minute/1440;
    while (day>=(unsigned)(365+(year%4==0))) day-=365+(year++%4==0);
    while (day>=(unsigned)month_days(year,month)) day-=month_days(year,month++);
    rb->lcd_putsxyf(8,y,"%s %04d-%02d-%02d %02u:%02u",label,year,month,day+1,
                    minute/60%24,minute%60);
}

static void review_screen(uint32_t now,unsigned sec,int choice)
{
    static const char *const reasons[]={"","Invalid RTC","Reset-era clock",
        "Time went backwards","Unexpected future date","Long offline interval","Clock changed while running"};
    invalidate_lcd=true;
    rb->lcd_clear_display();
    rb->lcd_putsxy(8,8,"Clock check - pet paused");
    rb->lcd_putsxy(8,30,reasons[pet.clock_hold]);
    stamp_line(53,"Saved",pet.last);
    stamp_line(72,"RTC  ",now);
    if (choice) {
        rb->lcd_putsxy(8,108,choice==1?"Discard this offline interval?":"Count this offline interval?");
        rb->lcd_putsxy(8,130,choice==1?"Keep pet as saved.":"Up to 7 days. Pet may die.");
        rb->lcd_putsxy(8,167,"SELECT: clock correct, confirm");
        rb->lcd_putsxy(8,189,"MENU: cancel");
    } else {
        rb->lcd_putsxy(8,108,"Check date/time in Rockbox.");
        rb->lcd_putsxy(8,136,"MENU: exit with pet paused");
        if (clock_can_resume(now,sec)) {
            rb->lcd_putsxy(8,163,"SELECT: review skip interval");
            if (clock_can_count(&pet,now,sec))
                rb->lcd_putsxy(8,185,"PLAY: review count interval");
        } else rb->lcd_putsxy(8,163,"Set clock first, then reopen.");
    }
    rb->lcd_update();
}

static ClockAnchor live_anchor;
static uint32_t previous_second;

static bool review_clock(enum plugin_status *stop)
{
    int choice=0,pressed=BUTTON_NONE;
    ClockAnchor decision={0,0};
    for (;;) {
        unsigned sec;
        uint32_t now=current_minute(&sec);
        if (choice && !clock_continuous(decision,now,sec,*rb->current_tick,HZ)) choice=0;
        review_screen(now,sec,choice);
        int btn=rb->button_get_w_tmo(HZ/2);
        if (btn==BUTTON_NONE) continue;
        if (btn==BUTTON_SELECT || btn==BUTTON_MENU || btn==BUTTON_PLAY) { pressed=btn; continue; }
        if (!(btn&BUTTON_REL) || (btn & ~BUTTON_REL)!=pressed) {
            pressed=BUTTON_NONE;
            if (rb->default_event_handler(btn)==SYS_USB_CONNECTED) { *stop=PLUGIN_USB_CONNECTED;return false; }
            continue;
        }
        int key=pressed;
        pressed=BUTTON_NONE;
        now=current_minute(&sec);
        if (key==BUTTON_MENU) {
            if (choice) { choice=0; continue; }
            *stop=PLUGIN_OK;
            tray_sync(&pet,true);
            return false;
        }
        if (!clock_can_resume(now,sec)) continue;
        if (!choice) {
            if (key==BUTTON_PLAY && !clock_can_count(&pet,now,sec)) continue;
            choice=key==BUTTON_PLAY?2:1;
            decision=(ClockAnchor){now*60+sec,*rb->current_tick};
        } else if (key==BUTTON_SELECT) {
            if (!clock_continuous(decision,now,sec,*rb->current_tick,HZ)) { choice=0;continue; }
            Pet candidate=pet;
            if (choice==1) clock_rebase(&candidate,now,sec);
            else {
                if (!clock_can_count(&candidate,now,sec)) { choice=0;continue; }
                candidate.clock_hold=CLOCK_OK;
                pet_clock(&candidate,now,sec);
            }
            uint32_t accepted_tick=*rb->current_tick;
            if (!commit_pet(&candidate)) {
                rb->splash(2*HZ,"Save failed - pet paused");
                choice=0;
                continue;
            }
            live_anchor=(ClockAnchor){now*60+sec,accepted_tick};
            previous_second=now*60+sec;
            pet=candidate;
            tray_sync(&pet,false);
            return true;
        }
    }
}

/* Return only trusted samples to the gameplay clock, on every entry path. */
static bool update_time(bool startup,bool trusted,enum plugin_status *stop)
{
    unsigned sec;
    uint32_t now=current_minute(&sec), tick=*rb->current_tick;
    enum ClockIssue issue=pet.clock_hold;
    if (!issue) {
        issue=clock_check_start(&pet,now,sec,trusted);
        if (!issue && !startup && (now*60+sec<previous_second ||
            !clock_continuous(live_anchor,now,sec,tick,HZ))) issue=CLOCK_LIVE_JUMP;
        if (issue) {
            pet.clock_hold=issue;
            if (!persist()) {
                rb->splash(2*HZ,"Clock hold save failed");
                *stop=PLUGIN_ERROR;
                return false;
            }
        }
    }
    if (issue) {
        tray_sync(&pet,false);
        if (!review_clock(stop)) return false;
        /* Rebase/count was committed to the minute and second confirmed in the dialog.
           Recheck the next RTC read against that decision rather than trusting a new sample. */
        return true;
    }
    if (startup || tick-live_anchor.ticks>=60u*HZ)
        live_anchor=(ClockAnchor){now*60+sec,tick};
    previous_second=now*60+sec;
    pet_clock(&pet,now,sec);
    return true;
}

enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    unsigned second;
    uint32_t now=current_minute(&second);
    enum plugin_status stop=PLUGIN_OK;
    invalidate_lcd=true;
    active_slot=-1;verified_slots=0;
    serial=0;
    const char *const *families[]={paths,journal_paths,daily_paths,family_paths,clock_paths,growth_paths,hatching_paths,legacy_paths};
    const char *const *source=paths;
    for (unsigned i=0;i<sizeof families/sizeof families[0];++i) {
        source=families[i];
        if (rb->file_exists(source[0]) || rb->file_exists(source[1])) break;
    }
    for (int i=0;i<2;++i) {
        Pet candidate;
        uint32_t seq;
        if (load_slot(source[i],&candidate,&seq) && (active_slot<0 || save_newer(seq,serial))) {
            active_slot=i; serial=seq; pet=candidate;
        }
    }
    if (active_slot<0) {
        if (rb->file_exists(source[0]) || rb->file_exists(source[1])) {
            rb->splash(2*HZ,"Both saves unreadable");
            return PLUGIN_ERROR;
        }
        if (now>MAX_CLOCK || second>=60) {
            rb->splash(2*HZ,"Set Rockbox date and time");
            return PLUGIN_ERROR;
        }
        pet_new(&pet,now);
    }
    unsigned previous=pet.character;
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_WHITE);
    rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    rb->lcd_setfont(FONT_SYSFIXED);
    uint32_t before_review=serial;
    if (!update_time(true,source==paths || source==journal_paths || source==daily_paths || source==family_paths || source==clock_paths,&stop)) return stop;
    if (serial==before_review && !persist()) {
        rb->splash(2*HZ,"Cannot write save"); return PLUGIN_ERROR;
    }
    tray_sync(&pet,false);
    Ui ui;
    ui_start(&ui);
    ui_growth(&ui,&pet,previous);
    unsigned saved_form=pet.character;
    ui.mode=rb->file_exists(CLASSIC_PREF)?CLASSIC_V3:NATIVE_IPOD;
    Sound sound={0};
    WheelInput input={0};
    bool redraw=true;
    long next=*rb->current_tick;
    long autosave=next+60*HZ;
    for (;;) {
        long tick=*rb->current_tick;
        if (!TIME_BEFORE(tick,next)) {
            unsigned previous_phase=pet.family.phase;
            unsigned last_minute=pet.last;
            Feedback before_tick=feedback_sample(&pet,&ui);
            if (!update_time(false,true,&stop)) return stop;
            if (pet.character!=saved_form) {
                ui_growth(&ui,&pet,saved_form);
                saved_form=pet.character;
                previous_phase=UINT32_MAX;
            }
            bool playing=ui.page==PLAYING && !ui.play.done;
            ui_tick(&ui,&pet);
            bool proposal_shown=before_tick.page!=OFFER && ui.page==OFFER;
            bool event=previous_phase!=pet.family.phase || proposal_shown || (playing && ui.play.done);
            if (event && !persist()) {
                rb->splash(HZ,"Cannot save progress");return PLUGIN_ERROR;
            }
            tray_after_tick(&pet,event);
            enum Cue cue=feedback_change(before_tick,&pet,&ui,CMD_NONE);
            if (cue!=SILENT) sound=(Sound){cue,0};
            if (pet.muted) sound=(Sound){0};
            if (sound.cue) {
                unsigned hz=sound_note(&sound);
                if (hz) rb->beep_play(hz, sound_duration(sound.cue), 400u<<pet.sound_level);
            }
            if (redraw || invalidate_lcd || last_minute!=pet.last || ui.page!=before_tick.page ||
                ui.page==HOME || ui.page==PLAYING || ui.page==WEDDING || ui.page==NOTICE ||
                (ui.page==CLERK && ui.knocks && ui.frame-ui.knock_start==16) ||
                ui.page==USING_ITEM || ui.page==OFFER || ui.page==DEPARTURE ||
                ui.page==DISPLAY_TEST || ui.page==JOURNAL || (pet_attention(&pet) && ui.frame%4==0))
                paint(&ui);
            redraw=false;
            /* Quantize the nominal 125 ms frames against Rockbox ticks without
               rounding every interval down (HZ=100 gives 12,13,12,13). */
            unsigned phase=ui.frame%8;
            next=tick+(long)(((phase+1)*HZ)/8-(phase*HZ)/8);
        }
        if (!TIME_BEFORE(tick,autosave)) {
            if (!persist()) {rb->splash(HZ,"Autosave failed");invalidate_lcd=true;}
            autosave=tick+60*HZ;
        }
        int btn=rb->button_get_w_tmo(MAX(1,next-*rb->current_tick));
        enum Control key;
        if (control_event(btn,&key)) {
            enum Phase phase=(btn&BUTTON_REL)?RELEASED:(btn&BUTTON_REPEAT)?REPEATED:PRESSED;
            enum Command cmd=wheel_command(&input,&ui,key,phase,(uint32_t)*rb->current_tick,HZ);
            if (cmd==CMD_NONE) continue;
            redraw=true;
            enum InputMode before=ui.mode;
            uint32_t before_input=serial;
            unsigned phase_before_input=pet.family.phase;
            if (!update_time(false,true,&stop)) return stop;
            if (before_input!=serial) { input=(WheelInput){0};continue; }
            static Pet before_action;
            before_action=pet;
            Feedback before_key=feedback_sample(&pet,&ui);
            ui_command(&ui,&pet,cmd);
            enum Cue cue=feedback_change(before_key,&pet,&ui,cmd);
            if (cue!=SILENT) sound=(Sound){cue,0};
            if (pet.muted) sound=(Sound){0};
            if ((phase_before_input!=pet.family.phase || rb->memcmp(&before_action,&pet,sizeof pet)) && !persist()) {
                rb->splash(HZ,"Cannot save progress");return PLUGIN_ERROR;
            }
            tray_after_input(&pet);
            if (ui.mode!=before && !store_mode(ui.mode)) {
                ui.mode=before;
                rb->splash(HZ,"Cannot save controls");invalidate_lcd=true;
            }
            if (!ui.exit_requested) {paint(&ui);redraw=false;}
            if (ui.exit_requested) {
                if (persist()) { tray_sync(&pet,true); return PLUGIN_OK; }
                ui.exit_requested=false;
                rb->splash(HZ,"Save failed; try again");invalidate_lcd=true;
            }
        } else {
            if ((btn&BUTTON_MAIN) && !(btn & (BUTTON_REL|BUTTON_REPEAT))) {
                /* A chord cancels pending short presses (including MENU). */
                input=(WheelInput){0};
            }
            if (rb->default_event_handler_ex(btn,on_system_exit,NULL)==SYS_USB_CONNECTED)
                return PLUGIN_USB_CONNECTED;
        }
    }
}
