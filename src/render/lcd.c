#include "lcd.h"
#include "../core/growth.h"
#ifdef MOCHI_LAYOUT_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Host-only check for other artwork entering a text cell (including spacing). */
static uint8_t text_cells[LCD_ROWS][LCD_COLS];
#endif
#include "fonts.inc"
#include "v3-drawings.inc"

static void block(Display *d,int x,int y,int w,int h)
{
#ifdef MOCHI_LAYOUT_TEST
    assert(x>=0 && y>=0 && x+w<=LCD_COLS && y+h<=LCD_ROWS);
#endif
    if (x<0 || y<0 || x+w>LCD_COLS || y+h>LCD_ROWS) return;
    for (int j=y;j<y+h;++j) {
        uint8_t *row=d->bits[j/8]+x;
        unsigned mask=1u<<(j%8);
        for (int i=0;i<w;++i) {
#ifdef MOCHI_LAYOUT_TEST
            assert(!text_cells[j][x+i]);
#endif
            row[i]|=mask;
        }
    }
}

static void stamp(Display *d,int x,int y,const uint8_t *rows,int w,int h,int scale)
{
    for (int j=0;j<h;++j)
        for (int i=0;i<w;++i)
            if (rows[j] & (1u<<(w-1-i)))
                block(d,x+i*scale,y+j*scale,scale,scale);
}

typedef struct { const uint8_t (*rows)[8]; int width,height; } Face;
static const Face compact={font_compact,5,7}, menu={font_menu,6,8}, wide={font_wide,8,8};

static int text_width(const char *s,const Face *f,int scale)
{
    int width=0;
    while (*s++) width+=(f->width+1)*scale;
    return width ? width-scale : 0;
}

static void write_text(Display *d,int x,int y,const char *s,const Face *f,int scale)
{
#ifdef MOCHI_LAYOUT_TEST
    int left=x, right=x+text_width(s,f,scale), bottom=y+f->height*scale;
    assert(left>=0 && y>=0 && right<=LCD_COLS && bottom<=LCD_ROWS);
    for (int j=y;j<bottom;++j)
        for (int i=left;i<right;++i)
            if ((d->bits[j/8][i] & (1u<<(j%8))) || text_cells[j][i]) {
                fprintf(stderr,"Layout overlap in %s at (%d,%d)\n",s,i,j);assert(0);
            }
#endif
    for (;*s;++s,x+=(f->width+1)*scale) {
        unsigned ch=(unsigned char)*s;
        if (ch<32 || ch>126) ch='?';
        const uint8_t *rows=f->rows[ch-32];
        for (int j=0;j<f->height;++j)
            for (int i=0;i<f->width;++i)
                if (rows[j] & (128u>>i)) block(d,x+i*scale,y+j*scale,scale,scale);
    }
#ifdef MOCHI_LAYOUT_TEST
    for (int j=y;j<bottom;++j) memset(&text_cells[j][left],1,right-left);
#endif
}

static void centered(Display *d,int y,const char *s,int scale)
{
    while (scale>1 && text_width(s,&menu,scale)>LCD_COLS-16) --scale;
    write_text(d,(LCD_COLS-text_width(s,&menu,scale))/2,y,s,&menu,scale);
}

static void value_line(Display *d,int y,const char *title,unsigned value)
{
    char buf[11];
    int i=10;
    buf[i]=0;
    do { buf[--i]=(char)('0'+value%10); value/=10; } while(value);
    write_text(d,24,y,title,&menu,3);
    write_text(d,LCD_COLS-24-text_width(buf+i,&menu,3),y,buf+i,&menu,3);
}

/* 12x12 silhouettes: scale, cutlery, toilet, game, IR; discipline, medicine,
   light, book, attention. Clear gutters remain on both sides of each symbol. */
static const uint16_t symbols[10][12]={
    {0x000,0x30c,0x79e,0xfff,0xfff,0xfff,0x7fe,0x3fc,0x1f8,0x0f0,0x060,0x000},
    {0x540,0x546,0x54e,0x55e,0x7de,0x39e,0x11e,0x106,0x106,0x106,0x106,0x000},
    {0x000,0x01e,0x012,0x012,0x01e,0x7fe,0x402,0x206,0x1fc,0x0d0,0x1f8,0x000},
    {0x000,0x000,0x3fc,0x7fe,0xe77,0xc03,0xe77,0xebb,0x783,0x300,0x000,0x000},
    {0x002,0x019,0x005,0x035,0x009,0x012,0x040,0x1e0,0x3c0,0x780,0x300,0x000},
    {0x0f0,0x1f8,0x30c,0x606,0x606,0x30c,0x198,0x0f0,0x000,0x0f0,0x0f0,0x060},
    {0x000,0x0f0,0x0f0,0x0f0,0x7fe,0x7fe,0x7fe,0x0f0,0x0f0,0x0f0,0x000,0x000},
    {0x018,0x039,0x0fa,0x398,0x71b,0x71b,0x398,0x1fa,0x1b9,0x198,0x0c0,0x0c0},
    {0x000,0x73e,0x8c1,0x8c1,0x8c1,0x8c1,0x8c1,0x8c1,0x8c1,0x73e,0x0c0,0x000},
    {0x060,0x0f0,0x0f0,0x0f0,0x0f0,0x060,0x060,0x000,0x060,0x0f0,0x060,0x000}
};
static void icon_strip(Display *d,int selected,bool alert)
{
    for (int i=0;i<10;++i) {
        int x=24+60*(i%5),y=i<5 ? 2 : 202;
        for (int j=0;j<12;++j)
            for (int k=0;k<12;++k)
                if (symbols[i][j] & (0x800u>>k)) block(d,x+3*k,y+3*j,3,3);
        if (i==IC_ATTENTION && alert) block(d,x+42,y+16,4,4);
        if (i==selected) {
            block(d,x-5,y-2,3,40); block(d,x+38,y-2,3,40);
            block(d,x-5,y-2,46,2); block(d,x-5,y+36,46,2);
        }
    }
    block(d,16,43,288,2);
    block(d,16,195,288,2);
}

static const uint8_t egg_art[]={24,60,126,90,126,60};
static const uint8_t bodies[][8]={
    {0,0,0,60,126,90,126,60},{0,0,60,126,90,126,126,36},
    {0,66,126,126,90,126,60,36},{66,102,126,126,90,126,126,36}
};
static void creature(Display *d,unsigned stage,bool asleep,unsigned frame,int x,int y)
{
    if (stage==EGG) { stamp(d,x,y,egg_art,8,6,6); return; }
    if (stage==DEAD) { centered(d,134,"MENU: New egg",3); return; }
    if (asleep) {
        stamp(d,x,y+12,(const uint8_t[]){60,126,126,90,126},8,5,6);
        return;
    }
    stamp(d,x,y,bodies[stage-1],8,8,6);
    block(d,x+((frame/4)%2 ? 6 : 36),y+48,6,6);
}

/* All offsets below are in LCD dot units. No filtering or subpixel motion. */
typedef struct { unsigned w,h; uint16_t rows[16]; } Sprite;
static const Sprite v3_art[]={
#include "v3-sprites.inc"
};
#include "v3-poses.inc"

/* One bit per source pixel. Sequence speed is an iPod playback choice.
   Fit the complete pose inside the caller's scene without clipping limbs. */
static bool sheet_pose(Display *d,unsigned id,enum SheetPose pose,unsigned phase,
                       int x,int y,int scale,int max_h,bool mirror)
{
    if (id>=V3_COUNT || pose>=SP_COUNT) return false;
    unsigned group=id*SP_COUNT+pose;
    unsigned start=pose_offsets[group], count=pose_offsets[group+1]-start;
    if (!count) return false;
    unsigned f=pose_ids[start+phase%count];
    int w=pose_frames[f].w,h=pose_frames[f].h;
    if (h*scale>max_h) scale=max_h/h;
    if (!scale) return false;
    const uint8_t *bits=pose_ink+pose_frames[f].offset;
    for (int row=0;row<h;++row) for (int col=0;col<w;++col) {
        unsigned bit=row*w+col;
        if (bits[bit/8]&(128u>>(bit%8)))
            block(d,x+scale*(mirror?w-1-col:col),y+scale*row,scale,scale);
    }
    return true;
}

enum Pose { IDLE_POSE, EAT_POSE, HAPPY_POSE, SICK_POSE, SAD_POSE, SLEEP_POSE, ROLL_POSE, TURN_POSE };
static void actor_as(Display *d,const Pet *p,unsigned id,enum Pose pose,unsigned frame,int x,int y,int scale)
{
    if (id==V3_LEGACY) {
        creature(d,p->stage,pet_sleeping(p),frame,x,y);
        return;
    }
    if (id==V3_EGG && p->egg_seconds<HATCH_SECONDS-4 &&
        sheet_pose(d,0,SP_EGG,p->egg_seconds>=4?frame/4:0,x,y,scale,60,false)) return;
    static const enum SheetPose action[]={SP_GENERAL,SP_EATING,SP_HAPPY,SP_SICK,SP_SAD,
                                          SP_IN_BED,SP_ROLLING,SP_BACK_TURNED};
    enum SheetPose selected=pose==IDLE_POSE && frame%29==0?SP_EYES_CLOSED:action[pose];
    if (id!=V3_EGG && sheet_pose(d,id,selected,frame/4,x,y,scale,60,
                                  pose==IDLE_POSE && frame/4%22>=11)) return;
    unsigned art=v3_characters[id].art;
    const Sprite *sprite=&v3_art[art];
    /* Integer coordinates of two eye dots on each actor's reference pose.
       Toggling the dots also works for a light eye on a dark face. */
    static const uint8_t eyes[][2]={
#include "v3-eyes.inc"
    };
    if (sprite->h && scale*(int)sprite->h>60) scale=60/(int)sprite->h;
    bool blink=art<sizeof eyes/sizeof eyes[0] && art>EGG_ART &&
               (pose==SLEEP_POSE || frame%29==0);
    bool beat=(frame/4)%2;
    bool egg=art==EGG_ART;
    bool split=egg && p->egg_seconds>=HATCH_SECONDS-4;
    bool rock=egg && p->egg_seconds>=4 && beat;
    for (unsigned row=0;row<sprite->h;++row) {
        for (unsigned col=0;col<sprite->w;++col) {
            bool ink=(sprite->rows[row] & (1u<<(sprite->w-1-col)))!=0;
            unsigned dot=(row<<4)|col;
            if (blink && (dot==eyes[art][0] || dot==eyes[art][1])) ink=!ink;
            if (!ink) continue;
            int dx=col,dy=row;
            if (egg) {
                if (rock && row<sprite->h/2) ++dx;
                if (split && row<5+col%3) dy-=1+(int)(p->egg_seconds-(HATCH_SECONDS-4));
            } else {
                if (pose==ROLL_POSE) {
                    unsigned rotation=frame/3%4;
                    if (rotation==1) {dx=sprite->h-1-row;dy=col;}
                    if (rotation==2) {dx=sprite->w-1-col;dy=sprite->h-1-row;}
                    if (rotation==3) {dx=row;dy=sprite->w-1-col;}
                }
                if (pose==TURN_POSE || (pose==IDLE_POSE && (frame/4)%22>=11)) dx=(int)sprite->w-1-dx;
                if (pose==SAD_POSE && beat) ++dy;
                if (pose==HAPPY_POSE && beat) dy-=2;
                if (pose==SICK_POSE && beat) ++dx;
                if (pose==SLEEP_POSE && beat) ++dy;
                if (pose==EAT_POSE && beat && row+2>=sprite->h) ++dy;
            }
            block(d,x+scale*dx,y+scale*dy,scale,scale);
        }
    }
    if (pose==SAD_POSE) block(d,x+(sprite->w+1)*scale,y+(3+(frame/2)%4)*scale,scale,scale*2);
}
static void food_symbol(Display *d,bool snack,int x,int y,int scale)
{
    static const uint8_t bowl[]={0x28,0x14,0,0xff,0x81,0x42,0x3c,0x18};
    static const uint8_t apple[]={0x0c,0x18,0x76,0xc3,0xc3,0x81,0x42,0x3c};
    stamp(d,x,y,snack?apple:bowl,8,8,scale);
}

static void actor(Display *d,const Pet *p,enum Pose pose,unsigned frame,int x,int y,int scale)
{
    actor_as(d,p,p->stage==DEAD?V3_LEGACY:p->character,pose,frame,x,y,scale);
}

static void activity(Display *d,const Ui *u,const Pet *p)
{
    const Play *g=&u->play;
    if (g->done) {
        centered(d,52,g->won?"WIN":"END",3);
        if (p->stage!=DEAD) actor(d,p,g->won?HAPPY_POSE:SAD_POSE,u->frame,128,84,3);
        value_line(d,134,"Score",g->score);
        value_line(d,161,"Gotchi",g->earned);
        return;
    }
    if (g->type==GET || g->type==HEADING) {
        int hop=g->type==HEADING && g->finish_ticks?(g->finish_ticks==1 || g->finish_ticks==4?7:16):0;
        int drop=g->type==HEADING && g->finish_ticks && g->round_won?115-4*(HEADING_JUMP_TICKS-g->finish_ticks):52+6*g->ticks;
        enum SheetPose falling=g->type==HEADING?SP_FOOTBALL:g->hazard?SP_DROPPINGS:SP_NOTES;
        sheet_pose(d,0,falling,g->type==GET && !g->hazard && g->double_note,64+g->target*84,drop,3,27,false);
        if (g->type==GET) {
            for (int lane=0;lane<3;++lane)
                sheet_pose(d,0,SP_BAGS,lane==g->lane,64+lane*84,155,3,30,false);
        } else actor(d,p,IDLE_POSE,u->frame,59+g->lane*84,150-hop,2);
    } else if (g->type==FLAG) {
        static const uint8_t flags[][5]={{31,31,31,16,16},{31,17,31,16,16}};
        if (g->hazard) stamp(d,151,108,(const uint8_t[]){7,7,4},3,3,6);
        else if (g->target==2) {
            stamp(d,110,102,flags[0],5,5,6);stamp(d,184,102,flags[1],5,5,6);
        } else {
            int side=g->target;
            if (g->score>=12 && g->round%3==0) side=1-side;
            stamp(d,side?184:110,102,flags[g->target],5,5,6);
        }
    } else if (g->type==MEMORY) {
        static const uint8_t arrows[][5]={{4,8,31,8,4},{4,14,21,4,4},{4,2,31,2,4}};
        int i=g->ticks/5;
        if (i>=g->length) centered(d,104,"COPY",4);
        else if (g->ticks%5<4) stamp(d,145,102,arrows[g->pattern[i]],5,5,6);
    } else if (g->type==SPRINT) {
        centered(d,62,"SPRINT",3);
        if (g->finish_ticks) {
            int progress=SPRINT_FINISH_TICKS-g->finish_ticks;
            int winner=192+6*progress;
            if (!sheet_pose(d,p->character,SP_RUNNING_AND_FALLING,g->round_won?progress%2:2,
                            g->round_won?winner:165,92,2,44,false))
                actor(d,p,g->round_won?HAPPY_POSE:SAD_POSE,u->frame,g->round_won?winner:165,92,2);
            sheet_pose(d,g->rival,SP_RUNNING_AND_FALLING,g->round_won?2:progress%2,
                       g->round_won?165:winner,144,2,44,false);
        } else {
            actor(d,p,HAPPY_POSE,u->frame,40+10*g->input,92,2);
            actor_as(d,p,g->rival,TURN_POSE,u->frame,40+10*(g->ticks*16/40),144,2);
        }
        for (int y=95;y<175;y+=10) block(d,235,y,4,5);
    } else {
        if (g->finish_ticks) {
            int push=(BUMP_CLASH_TICKS-g->finish_ticks)%2*8;
            bool fall=g->finish_ticks<=2;
            if (!fall || g->round_won || !sheet_pose(d,p->character,SP_RUNNING_AND_FALLING,2,116+push,115,3,56,false))
                actor(d,p,TURN_POSE,u->frame,116+push,115,3);
            if (!fall || !g->round_won || !sheet_pose(d,g->rival,SP_RUNNING_AND_FALLING,2,170-push,115,3,56,false))
                actor_as(d,p,g->rival,IDLE_POSE,u->frame,170-push,115,3);
            return;
        }
        centered(d,52,g->answered?"POWER":g->ticks>=BUMP_ENTRY_TICKS?"PUSH B":"BUMP",3);
        for (int i=0;i<g->power;++i) block(d,80+i*32,84,26,9);
        int approach=g->answered?22:g->ticks<8?g->ticks*3:22;
        actor(d,p,TURN_POSE,u->frame,57+approach,115,3);
        actor_as(d,p,g->rival,IDLE_POSE,u->frame,211-approach,115,3);
    }
}

static void name_text(char out[6],uint32_t code)
{
    for (int i=0;i<5;++i) out[i]=name_letter(code,i);
    out[5]=0;
}

static void status(Display *d,const Pet *p,int choice)
{
    if (choice==0) {
        static const uint8_t heart[][6]={{54,73,65,34,20,8},{54,127,127,62,28,8}};
        const unsigned levels[]={p->hunger,p->happy};
        for (int i=0;i<2;++i) {
            int y=76+i*64;
            write_text(d,24,y,i?"Happy":"Food",&menu,3);
            for (unsigned j=0;j<4;++j)
                stamp(d,152+36*j,y,heart[j<levels[i]],7,6,4);
        }
    } else if (p->character!=V3_LEGACY && choice<=6) {
        char name[6];name_text(name,choice==5?p->user_name:p->pet_name);
        switch (choice) {
        case 1:value_line(d,108,"Training",p->training);break;
        case 2:value_line(d,54,"Age",p->minutes/1440);value_line(d,96,"Lb",p->weight);centered(d,143,name,3);break;
        case 3:centered(d,77,p->sex?"Girl":"Boy",3);value_line(d,135,"Gen",p->generation);break;
        case 4:value_line(d,108,"Points",p->points);break;
        case 5:centered(d,70,"User name",2);centered(d,129,name,4);break;
        case 6:centered(d,76,pet_character(p),3);centered(d,145,pet_stage(p),2);break;
        }
    } else if (choice==1) {
        value_line(d,76,"Age",p->minutes/1440); value_line(d,140,"Lb",p->weight);
    } else if (choice==2) {
        value_line(d,76,"Training",p->training); centered(d,140,pet_stage(p),3);
    } else if (choice==3) value_line(d,108,"Points",p->points);
    else if (choice==4) {
        centered(d,82,pet_character(p),3);
        if (p->character==V3_LEGACY) centered(d,145,"Legacy profile",2);

        else value_line(d,145,"Gen",p->generation);
    }
    else if (choice>=7) {
        static const char *const labels[]={"Baby history","Toddler history","Teen history","Adult history"};
        unsigned id=history_at(p,(enum Stage)(BABY+choice-7));
        centered(d,70,labels[choice-7],3);
        centered(d,135,id?v3_characters[id].name:"No record",3);
    } else {
        char name[6];
        name_text(name,choice==5?p->pet_name:p->user_name);
        centered(d,67,choice==6?"User":p->sex?"Girl":"Boy",3);
        centered(d,133,name,4);
    }
}

/* Item and souvenir front views use their original single-bit grids. */
static void goods_picture(Display *d,unsigned id,int cx,int top,int scale)
{
    static const struct {unsigned char w,h;uint32_t rows[32];} art[]={
#include "goods-art.inc"
    };
    if (id>=sizeof art/sizeof art[0]) return;
    unsigned w=art[id].w,h=art[id].h;
    for (unsigned y=0;y<h;++y) for (unsigned x=0;x<w;++x)
        if (art[id].rows[y]&(1u<<(w-1-x))) block(d,cx-(int)w*scale/2+(int)x*scale,top+(int)y*scale,scale,scale);
}

#include "v3-ball.inc"

#include "events.inc"
static void timed_scene(Display *d,const uint32_t frames[][30],const unsigned *ends,
                        unsigned count,unsigned elapsed,int x,int y,int scale)
{
    unsigned i=0;
    while (i+1<count && elapsed>=ends[i]) ++i;
    for (int row=0;row<30;++row) for (int col=0;col<32;++col)
        if (frames[i][row]&(1u<<(31-col))) block(d,x+scale*col,y+scale*row,scale,scale);
}
#define FRAME_COUNT(a) (sizeof(a)/sizeof((a)[0]))

static void routine_props(Display *d,enum Routine scene,unsigned frame,int x,int y)
{
    int beat=frame/3%2;
    switch (scene) {
    case YAWN:
        stamp(d,x-12,y+12+beat*3,(const uint8_t[]){6,9,9,6},4,4,3);
        stamp(d,x-26,y-3-beat*3,(const uint8_t[]){4,2,4},3,3,2);break;
    case BRUSH:
        stamp(d,x-14+beat*6,y+21,(const uint8_t[]){7,127,64},7,3,3);break;
    case BATH:
        stamp(d,x-2,y+29,(const uint8_t[]){255,129,129,126,66},8,5,6);
        stamp(d,x+8,y-9-beat*3,(const uint8_t[]){5,10,5},4,3,3);break;
    case SING: case SIGNAL:
        stamp(d,x+54,y+4-beat*5,scene==SING?(const uint8_t[]){7,4,4,12,12}:
              (const uint8_t[]){8,4,18,4,8},5,5,3);break;
    case SNOWMAN: case BUNNY: case PUMPKIN: case TREE: case CAKE: case SCHOOL: case FEAST: {
        static const uint8_t seasonal[][8]={
            {24,60,36,24,126,90,126,60},{66,102,102,60,90,126,36,102},
            {12,24,126,219,255,165,126,60},{24,60,24,126,60,255,24,24},
            {36,36,126,90,126,66,126,255},{24,60,126,255,90,126,102,102},
            {129,195,102,60,126,255,126,36}
        };
        static const enum SheetPose reference[]={SP_WINTER,SP_EASTER,SP_HALLOWEEN,SP_TREE,
                                                SP_BIRTHDAY,SP_COUNT,SP_THANKSGIVING};
        if (sheet_pose(d,0,reference[scene-SNOWMAN],frame/4,27,108,2,62,false)) break;
        stamp(d,46,118-beat*3,seasonal[scene-SNOWMAN],8,8,3);break;
    }
    case WATCH:
        stamp(d,x-38,y+7,(const uint8_t[]){10,4,31,17,21,17,31},5,7,4);break;
    default:break;
    }
}

static enum SheetPose toy_pose(unsigned item)
{
    switch (item) {
    case PEN:case PENCIL:return SP_PEN_PENCIL;
    case BOW:return SP_BOW;
    case BOW_TIE:return SP_BOW_TIE;
    case CAP:return SP_CAP;
    case DARTS:return SP_DARTS;
    case DRUM:return SP_DRUM;
    case HAIR_GEL:return SP_HAIR_GEL;
    case MAKE_UP:return SP_MAKEUP;
    case MUSIC:return SP_MUSIC;
    case RC_CAR_1:case RC_CAR_2:case RC_TOY:return SP_REMOTE_CONTROL;
    case ROLLER_SKATES:return SP_ROLLER_SKATES;
    case ROYAL_COSTUME:return SP_ROYAL_COSTUME;
    case SHIRT:return SP_SHIRT;
    case SHOES:return SP_SHOES;
    case SUNGLASSES:return SP_SUNGLASSES;
    case THRONE:return SP_THRONE;
    case TRUMPET:return SP_TRUMPET;
    case UMBRELLA:return SP_UMBRELLA;
    case WEIGHTS:return SP_WEIGHTS;
    case WIG:return SP_WIG;
    case WINGS:return SP_WINGS;
    default:return SP_COUNT;
    }
}

static void care_picture(Display *d,const Pet *p,const Ui *u)
{
    enum Care action=(enum Care)(u->care_scene-1);
    unsigned beat=u->frame/2%2;
    if (action==MEDICINE && u->detail)
        actor_as(d,p,p->family.child_sex?V3_TELETCHI_GIRL:V3_TELETCHI_BOY,
                 p->family.child_doses?SICK_POSE:HAPPY_POSE,u->frame,146,103,4);
    else if (action!=TIME_OUT || !sheet_pose(d,p->character,SP_ANGRY,0,146,103,3,55,false))
        actor(d,p,action==MEAL||action==SNACK?EAT_POSE:action==TIME_OUT?TURN_POSE:
          action==LIGHT && pet_sleeping(p)?SLEEP_POSE:HAPPY_POSE,u->frame,146,103,3);
    switch (action) {
    case MEAL:case SNACK:
        if (u->item<GOODS_COUNT) goods_picture(d,u->item,106+beat*17,126,2);
        else food_symbol(d,action==SNACK,95+beat*14,128,3);
        break;
    case FLUSH:
        if (u->detail) sheet_pose(d,0,SP_TOILET,p->stage>=TEEN,127,141,2,43,false);
        else sheet_pose(d,0,SP_WASH,0,45+(12-u->frames)*9,111,2,64,false);
        break;
    case MEDICINE:
        stamp(d,95+beat*19,123,(const uint8_t[]){14,17,31,17,14},5,5,4);
        if (u->detail?p->family.child_doses:p->illness) centered(d,163,"One more dose",2);
        break;
    case PRAISE:
        stamp(d,208,107-beat*7,(const uint8_t[]){54,127,127,62,28,8},7,6,3);break;
    case TIME_OUT:
        stamp(d,122,99,(const uint8_t[]){4,4,4,0,4},3,5,4);break;
    case LIGHT:
        centered(d,162,p->dark?"Off":"On",2);
        if (p->dark) for (int y=12;y<19;++y) for (int x=87;x<236;++x) d->bits[y][x]^=255;
        break;
    }
}

void draw_frame(Display *d,const Pet *p,const Ui *u)
{
    *d=(Display){0};
#ifdef MOCHI_LAYOUT_TEST
    memset(text_cells,0,sizeof text_cells);
#endif
    icon_strip(d,u->page==HOME ? u->icon : -1,pet_attention(p) && (u->frame/4)%2);
    if (p->setup==REVEAL_BABY && u->page==HOME) {
        centered(d,60,p->sex?"It is a girl!":"It is a boy!",3);
        if (!sheet_pose(d,p->character,SP_HATCHING,0,137,95,4,60,false))
            actor(d,p,HAPPY_POSE,u->frame,140,105,5);
        centered(d,163,"Center: name",2);
        return;
    }
    if (p->setup && u->page==HOME) {
        if (p->setup<=2) value_line(d,106,p->setup==1?"Birth month":"Birth day",
                                    p->setup==1?p->birthday_month:p->birthday_day);
        else {
            char name[6];
            name_text(name,p->setup<8?p->user_name:p->pet_name);
            centered(d,66,p->setup<8?"User name":"Pet name",3);
            centered(d,126,name,4);
            int left=(LCD_COLS-text_width(name,&menu,4))/2;
            block(d,left+28*((p->setup-3)%5),164,24,4);
        }
        return;
    }
    if (p->paused && u->page==HOME) { centered(d,90,"PAUSE",4); centered(d,144,"MENU: options",2); return; }
    static const char *const games[]={"GET","BUMP","FLAG","HEADING","MEMORY","SPRINT"};
    switch (u->page) {
    case OFFER: case WEDDING: case DEPARTURE: {
        unsigned other;
        int beat=(u->frame/4)%2;
        if (u->page==OFFER) {
            centered(d,52,is_senior(p)?"Local elder pair":"Matchmaker",3);
            if (is_senior(p)) actor(d,p,IDLE_POSE,u->frame,66,85,3);
            else timed_scene(d,matchmaker_rows,matchmaker_ends,FRAME_COUNT(matchmaker_ends),
                        (u->frame-u->offer_frame)*125u%matchmaker_ends[FRAME_COUNT(matchmaker_ends)-1],44,71,2);
            other=p->family.mate;
            actor_as(d,p,other,HAPPY_POSE,u->frame,194,85,3);
            centered(d,136,v3_characters[other].name,2);
            centered(d,155,"Love?",2);
            centered(d,175,"Center: Yes  MENU: No",2);
        } else if (u->page==WEDDING && u->frames>24) {
            centered(d,54,"Wedding",3);other=p->family.mate;
            if (!sheet_pose(d,p->character,SP_KISSING,0,75+beat*5,101,4,60,false))
                actor(d,p,HAPPY_POSE,u->frame,75+beat*5,101,4);
            if (!sheet_pose(d,other,SP_KISSING,0,183-beat*5,101,4,60,true))
                actor_as(d,p,other,HAPPY_POSE,u->frame,183-beat*5,101,4);
            stamp(d,150,85,(const uint8_t[]){54,127,127,62,28,8},7,6,2);
        } else {
            bool leaving=u->page==DEPARTURE;
            centered(d,52,leaving?"Parent leaving":"A baby!",3);
            int walk=leaving?(int)(u->frame%30):0;
            actor(d,p,IDLE_POSE,u->frame,90-walk*2,99,4);
            other=p->family.child_sex?V3_TELETCHI_GIRL:V3_TELETCHI_BOY;
            actor_as(d,p,other,HAPPY_POSE,u->frame,171,125,3);
            centered(d,162,leaving?(p->family.event_age?"Baby wakes at 08:00":"Center: take over"):p->family.child_sex?"Girl":"Boy",2);
        }
        break;
    }
    case JOURNAL: {
        unsigned id=V3_TELETCHI_BOY+u->choice;
        bool known=p->journal.seen[id/32]&(1u<<(id%32));
        unsigned entry=p->journal.raised[id];
        centered(d,50,known?v3_characters[id].name:"Undiscovered",3);
        if (known && u->detail) {
            static const char *const stages[]={"Baby","Toddler","Teen","Adult","Senior","Costume","Special"};
            centered(d,87,stages[v3_characters[id].life_class],2);
            value_line(d,112,"Raised",entry>>16);
            if (entry) value_line(d,144,"First gen",entry&65535);
            else centered(d,144,"Seen only",2);
        } else if (known) {
            actor_as(d,p,id,IDLE_POSE,u->frame,138,90,4);
            centered(d,157,entry?"Raised":"Encountered",2);
        } else centered(d,103,"?",4);
        break;
    }
    case PROGRESS: {
        unsigned raised=0,souvenirs=0,secrets=0;
        for (unsigned i=2;i<V3_COUNT;++i) raised+=p->journal.raised[i]!=0;
        for(unsigned i=0;i<32;++i) souvenirs+=(p->shop.souvenirs>>i)&1u;
        for(unsigned i=0;i<7;++i) secrets+=(p->shop.secrets>>i)&1u;
        value_line(d,50,"Gen",p->generation);value_line(d,82,"Raised",raised);
        value_line(d,114,"Souvenirs",souvenirs);value_line(d,146,"Codes",secrets);break;
    }
    case NOTEBOOK:
        centered(d,64,"Notebook",3);
        centered(d,119,((const char*[]){"Parents","History","Donate","Local elder pair","Character book","Collection"})[u->choice],2);break;
    case MEMORIAL:
        if (!p->memorial_count) centered(d,104,"No history yet",3);
        else {
            const uint32_t *a=p->memorial[u->choice];
            char n[6];name_text(n,a[0]);value_line(d,55,"Gen",a[2]);
            centered(d,96,n,3);centered(d,140,v3_characters[a[1]].name,2);
        }
        break;
    case DONATION:
        value_line(d,53,"Given",p->shop.donated);value_line(d,91,"Entry",u->amount);
        value_line(d,130,"Digit",u->choice);break;
    case DONATE_ASK:
        value_line(d,67,"Give",u->amount);
        centered(d,118,"Center: Yes",3);centered(d,152,"MENU: No",3);break;
    case CLERK: {
        unsigned item=shelf_item(p,0);
        unsigned sale=shelf_price(p,item)<goods_prices[item]?2:0;
        centered(d,54,"Shop",3);
        sheet_pose(d,0,SP_SHOP_STALL,sale,34,87,2,60,false);
        write_text(d,117,102,"Center: browse",&menu,2);
        centered(d,155,u->knocks && u->frame-u->knock_start<16?"Quick: three taps":"Next: call clerk",2);break;
    }
    case SHOP_CODE: case PASSWORD: {
        if (u->page==PASSWORD)
            sheet_pose(d,0,SP_PASS_STALL,0,33,121,2,60,false);
        else
            sheet_pose(d,0,SP_SHOP_STALL,1,33,121,2,60,false);
        char digit[2]={(char)((u->page==PASSWORD?'0':'A')+u->choice),0};
        centered(d,56,u->page==PASSWORD?"Password":"Shop code",3);
        centered(d,101,u->entry,3);centered(d,141,digit,3);break;
    }
    case CLAIM: {
        unsigned id=code_reward(p,u->code);
        centered(d,54,id<GOODS_COUNT?goods_names[id]:souvenir_names[id-GOODS_COUNT],2);
        value_line(d,84,"Price",code_price(p,u->code));
        centered(d,128,"Center: Yes",3);centered(d,162,"MENU: No",3);break;
    }
    case SOUVENIRS: {
        char receipt[11];
        centered(d,56,souvenir_names[u->choice],2);
        if (p->shop.souvenirs&(1u<<u->choice)) goods_picture(d,GOODS_COUNT+u->choice,160,93,2);
        else if (town_receipt(p,u->choice,receipt)) {
            centered(d,91,"Local desk password",2);
            char halves[]="00000 00000";
            for (int i=0;i<10;++i) halves[i+(i>=5)]=receipt[i];
            centered(d,124,halves,3);
            centered(d,160,"Enter in Password",2);
        } else centered(d,116,"Not collected",2);
        break;
    }
    case LINEAGE:
        if (!p->family.count) centered(d,104,"No parents yet",3);
        else {
            const Ancestor *a=&p->family.records[u->choice];
            if (u->detail==1) {
                centered(d,54,a->method==1?"Matchmaker":"Local elder pair",2);
                centered(d,91,a->sex?"Mother + father":"Father + mother",2);
                centered(d,125,a->child_sex?"Baby: girl":"Baby: boy",2);
                centered(d,151,"Mate name: unknown",2);
                break;
            }
            if (u->detail==2) {
                unsigned day,year,month;
                civil_date(a->birth,&year,&month,&day);
                char date[]="0000-00-00 00:00";
                unsigned values[]={year/100,year%100,month,day,a->birth/60%24,a->birth%60};
                static const unsigned starts[]={0,2,5,8,11,14};
                for (unsigned i=0;i<6;++i) {
                    date[starts[i]]='0'+values[i]/10;date[starts[i]+1]='0'+values[i]%10;
                }
                centered(d,69,"Baby born",3);centered(d,120,date,2);break;
            }
            char parent_name[6];name_text(parent_name,a->name);
            value_line(d,51,"Gen",p->generation-1-u->choice);
            centered(d,84,parent_name,3);
            centered(d,113,v3_characters[a->form].name,2);
            centered(d,136,"Partner",2);
            centered(d,158,v3_characters[a->mate].name,2);
        }
        break;
    case OPTIONS:
        centered(d,60,"Menu",2);
        centered(d,112,((const char*[]){"Continue","Settings","Clock","Save & exit","New game"})[u->choice],3);
        break;
    case SETTINGS:
        centered(d,60,"Settings",2);
        centered(d,112,((const char*[]){"Controls","Display test",p->muted?"Sound: off":"Sound: on",p->paused?"Resume pet":"Pause pet",((const char*[]){"Volume: low","Volume: medium","Volume: high"})[p->sound_level]})[u->choice],3);
        break;
    case INPUT_MODE:
        centered(d,60,"Controls",2);
        centered(d,112,u->choice==CLASSIC_V3?"Classic V3":"Native iPod",3);
        break;
    case EXIT_ASK: case HATCH_ASK: case REPLACE_ASK:
        centered(d,68,u->page==EXIT_ASK?"Save and exit?":u->page==HATCH_ASK?"Start new game?":"Replace this pet?",3);
        centered(d,118,"Center: Yes",3);
        centered(d,152,"MENU: No",3);
        break;
    case DISPLAY_TEST: draw_display_test(d,u->choice,u->frame); return;
    case TITLE:
        centered(d,65,"MOCHIPOD",4);
        if (p->character==V3_EGG) actor(d,p,IDLE_POSE,0,140,111,4);
        break;
    case HOME: {
        bool asleep=pet_sleeping(p);
        enum Routine routine=pet_routine(p);
        if (p->character==V3_EGG) {
            centered(d,57,"Egg",3);
            actor(d,p,IDLE_POSE,u->frame,135,109,5);
            break;
        }
        if (u->growth_frames && p->stage>=CHILD && p->stage<=ADULT) {
            unsigned form=p->character;
            if (u->growth_frames>10 && (u->growth_frames/3)%2)
                form=u->growth_from;
            centered(d,53,"Growing",3);
            actor_as(d,p,form,HAPPY_POSE,u->frame,135,91,4);
            centered(d,164,pet_character(p),2);
            break;
        }
        if (p->stage==DEAD) {
            centered(d,52,"Farewell",3);
            sheet_pose(d,0,SP_ANGEL,0,136,94,4,56,false);
            centered(d,156,"MENU: New game",2);break;
        }
        if (u->icon>=0) centered(d,52,icon_names[u->icon],3);
        if (routine==NEW_YEAR) {
            unsigned elapsed=u->frame-u->minute_frame;
            sheet_pose(d,0,elapsed<32?SP_FIREWORKS:SP_NEW_YEAR,elapsed/4,112,84,3,96,false);
            break;
        }
        if (asleep && routine!=SANTA_VISIT) {
            unsigned wake=pet_hours(p).wake;
            if (p->stage==BABY) centered(d,82,"Nap",2);
            else {
                char label[]="Wake 00:00";
                label[5]+=(wake/60)/10;label[6]+=(wake/60)%10;
                centered(d,82,label,2);
            }
            write_text(d,246,104,(u->frame/4)%2?"Zz":"Z",&menu,2);
        }
        if (!asleep && p->illness)
            sheet_pose(d,0,p->cavities?SP_TOOTH:SP_SKULL,p->stage!=BABY,33,112,2,20,false);
        if (!asleep && (p->illness || p->call))
            write_text(d,21,88,p->illness?(p->cavities?"Tooth":"Sick"):p->call==CRYING?"Sad":"Hey",&menu,2);
        unsigned step=(u->frame/4)%22;
        int walk=step<=11?(int)step:22-(int)step;
        unsigned dressed=p->character;
        if (p->shop.worn==CAT_COSTUME || p->shop.worn==OWL_COSTUME)
            dressed=p->shop.worn==CAT_COSTUME?V3_NYATCHI:V3_HOHOTCHI;

        int x=asleep?136:94+6*walk;
        if (routine==RUN) x=86+(u->frame%26)*4;
        if (routine==BATH || routine==BRUSH) x=135;
        int y=asleep?116:108+3*((u->frame/4)%2);
        if (routine==CLOSE_UP && !p->mess && p->family.phase==SINGLE) {
            if (sheet_pose(d,dressed,SP_CLOSE_UP,u->frame/4,112,86,3,96,false)) break;
        }
        if (routine==BUNNY && dressed==V3_HANATCHI && !p->mess && p->family.phase==SINGLE) {
            timed_scene(d,easter_rows,easter_ends,FRAME_COUNT(easter_ends),
                        (u->frame-u->minute_frame)*125u%easter_ends[FRAME_COUNT(easter_ends)-1],112,90,3);
            break;
        }
        enum SheetPose daily=SP_COUNT;
        switch (routine) {
        case BATH: daily=SP_BATH;break;
        case BRUSH: daily=SP_BRUSHING_TEETH;break;
        case SCHOOL: daily=SP_BACKPACK;break;
        case SING: daily=SP_SINGING;break;
        case RUN: daily=SP_RUNNING_AND_FALLING;break;
        default: break;
        }
        if (p->shop.worn==ROYAL_COSTUME && !asleep && routine==ROAM) daily=SP_ROYAL_COSTUME;
        if (p->call==NAUGHTY && !asleep && !p->illness) daily=SP_BADLY_BEHAVED;
        bool depicted=sheet_pose(d,dressed,daily,routine==RUN?u->frame/4%2:u->frame/4,x,y,3,60,false);
        if (!depicted)
            actor_as(d,p,dressed,asleep?SLEEP_POSE:p->illness?SICK_POSE:p->call==CRYING && p->stage==TEEN?TURN_POSE:(p->call==CRYING || !p->happy)?SAD_POSE:routine==ROLL?ROLL_POSE:routine==DANCE?HAPPY_POSE:p->call==NAUGHTY?TURN_POSE:IDLE_POSE,u->frame,x,y,5);
        if (routine==SANTA_VISIT) {
            unsigned elapsed=u->frame-u->minute_frame;
            if (p->last%1440==1350 && elapsed<16) {
                sheet_pose(d,0,SP_SANTA,elapsed/6,60,82+2*(int)elapsed,3,50,false);
            } else {
                static const uint8_t gift[]={0x24,0x3c,0xff,0x18,0xff,0x99,0x99,0xff};
                stamp(d,86,133,gift,8,8,3);
            }
        }
        if (!depicted) routine_props(d,routine,u->frame-u->minute_frame,x,y);
        if (pet_needs_toilet(p)) stamp(d,x-19,y+18,(const uint8_t[]){5,10,5,10},4,4,3);
        if (p->family.phase==WITH_BABY) {
            unsigned baby=p->family.child_sex?V3_TELETCHI_GIRL:V3_TELETCHI_BOY;
            actor_as(d,p,baby,asleep?SLEEP_POSE:p->family.child_doses?SICK_POSE:IDLE_POSE,u->frame,245,140,3);
            if (!asleep && p->family.child_doses)
                sheet_pose(d,0,SP_SKULL,0,251,118,3,18,false);
        }
        for (unsigned i=0;i<p->mess;++i) sheet_pose(d,0,SP_DROPPINGS,1,23+(i%4)*18,153+(i/4)*17,2,16,false);
        if (p->dark && routine!=SANTA_VISIT) {
            for (int y=48/8;y<192/8;++y)
                for (int x=16;x<304;++x) d->bits[y][x]^=255;
        }
        break;
    }
    case STATUS: status(d,p,u->choice); break;
    case CLOCK: {
        unsigned hour=p->last/60%24,minute=p->last%60;
        char clock_text[]={(char)('0'+hour/10),(char)('0'+hour%10),':',
                           (char)('0'+minute/10),(char)('0'+minute%10),0};
        centered(d,65,"Clock",3); centered(d,125,clock_text,5); break;
    }
    case ACTIV_MENU:
        centered(d,104,((const char*[]){"Game","Item","Shop","Password","Souvenir"})[u->choice],4);break;
    case SHOP: {
        unsigned id=shelf_item(p,u->choice);
        centered(d,52,goods_names[id],2);goods_picture(d,id,160,70,2);
        const char *gate=item_gate(p,id);
        if (gate) centered(d,110,gate,2);
        value_line(d,130,"Wallet",p->points);
        if (p->sold&(1u<<u->choice)) centered(d,156,"SOLD",3);
        else value_line(d,156,shelf_price(p,id)<goods_prices[id]?"Sale":"Price",shelf_price(p,id));
        break;
    }
    case ITEMS: {
        unsigned id=owned_item(p,u->choice,false,false);
        if (id==GOODS_COUNT) centered(d,105,"No items",3);
        else {
            centered(d,52,goods_names[id],2);goods_picture(d,id,160,80,2);
            value_line(d,148,"Owned",goods_owned(p,id));
        }
        break;
    }
    case USING_ITEM:
        centered(d,53,goods_names[u->item],3);
        if (u->item==BALL && p->character==V3_HINOTAMATCHI) {
            timed_scene(d,ball_frames,ball_end_ms,FRAME_COUNT(ball_end_ms),(unsigned)(71-u->frames)*125,112,84,3);
        } else if (goods_kind[u->item]==TICKET) {
            if (u->frames>16) {
                stamp(d,47+10*(32-u->frames),110-(32-u->frames),(const uint8_t[]){8,12,63,12,8},6,5,4);
                block(d,39,139,115,3);
            } else {
                unsigned dest=u->item==SWITZERLAND?9:u->item==HAWAII?10:u->item==AUSTRALIA?11:u->item==CHINA?12:13;
                goods_picture(d,GOODS_COUNT+dest,159,81,2);
            }
            centered(d,152,"Code in Souvenirs",2);
        } else if (u->item==MUSIC_DISC) {
            actor(d,p,u->frames>16?HAPPY_POSE:SAD_POSE,u->frame,170,103,3);
            if (u->frames>16) goods_picture(d,BOOM_BOX,104,123,2);
            else stamp(d,92,103,(const uint8_t[]){65,42,28,119,28,42,65},7,7,3);
        } else if (u->item==FISHING_POLE) {
            unsigned beat=(u->frame/2)%3;
            if (u->frames<=16 || !sheet_pose(d,p->character,SP_FISHING,0,171,100,3,60,false))
                actor(d,p,u->frames<=16 && u->caught==TIN_CAN?SAD_POSE:IDLE_POSE,u->frame,171,100,3);
            goods_picture(d,FISHING_POLE,115,100,2);
            block(d,88,113,2,17+beat);
            if (u->frames<=16) {
                static const uint8_t fish[]={8,61,123,127,61,8};
                static const uint8_t can[]={31,17,31,17,31};
                if (u->caught==POINT_BAG) value_line(d,155,"Gotchi",u->amount);
                else if (u->caught==TIN_CAN) stamp(d,82,131,can,5,5,3);
                else if (u->caught==BIG_FISH || u->caught==SMALL_FISH)
                    stamp(d,72,131,fish,7,6,u->caught==BIG_FISH?5:2);
            }
        } else if ((u->item==PEN || u->item==PENCIL) && u->frames<=16 &&
                   !drawings[p->character].bits && sheet_pose(d,p->character,SP_PAPER,0,130,100,3,66,false)) {
            break;
        } else if (((u->item!=PEN && u->item!=PENCIL) || u->frames>16) &&
                   sheet_pose(d,p->character,toy_pose(u->item),(32-u->frames)/4,125,93,3,72,false)) {
            if (toy_pose(u->item)==SP_REMOTE_CONTROL)
                goods_picture(d,u->item,68+(32-u->frames),143,2);
        } else if (u->item==BALL || u->item==DRUM ||
                   u->item==TRUMPET || u->item==BOOM_BOX) {
            int beat=(u->frame/2)%4;
            actor(d,p,u->item==BALL?HAPPY_POSE:IDLE_POSE,u->frame,149,109,4);
            if (u->item==BALL) goods_picture(d,BALL,100+beat*9,128-(beat==1||beat==2?21:0),2);
            else {
                goods_picture(d,u->item,108,113+beat,2);
                routine_props(d,SING,u->frame,54,88);
            }
        } else if ((u->item==PEN || u->item==PENCIL) && drawings[p->character].bits && u->frames<=16) {
            unsigned width=drawings[p->character].width, height=drawings[p->character].height;
            const uint8_t *rows=drawings[p->character].bits;
            for (unsigned y=0;y<height;++y)
                for (unsigned x=0;x<width;++x)
                    if (rows[y*((width+7)/8)+x/8] & (128u>>(x%8)))
                        block(d,160-(int)width/2+(int)x,93+(int)y,1,1);
        } else if (u->item==BUILDING_BLOCKS) {
            unsigned pieces=(32-u->frames)/6+1;
            actor(d,p,HAPPY_POSE,u->frame,145,105,4);
            for (unsigned n=0;n<pieces;++n) {
                int x=71+12*(n%3), y=145-12*(n/3);
                block(d,x,y,11,11); /* separate blocks remain visible in silhouette */
            }
        } else if (u->item==RC_CAR_1 || u->item==RC_CAR_2 || u->item==RC_TOY) {
            actor(d,p,IDLE_POSE,u->frame,137,89,3);
            goods_picture(d,u->item,62+5*(32-u->frames),137,2);
        } else if (u->item==WEIGHTS) {
            int lift=128-(u->frame%8<4?26:0);
            actor(d,p,HAPPY_POSE,u->frame,133,109,4);
            goods_picture(d,WEIGHTS,107,lift,2);goods_picture(d,WEIGHTS,204,lift,2);
        } else if (u->item==WINGS || u->item==UMBRELLA || u->item==SUNGLASSES ||
                   u->item==WIG || u->item==CAP || u->item==BOW || u->item==SHIRT) {
            int x=112+(u->frame%16)*2, y=u->item==WINGS?104+(u->frame/2%6)*3:116;
            actor(d,p,HAPPY_POSE,u->frame,x,y,3);
            int top=u->item==SHIRT?y+17:u->item==SUNGLASSES?y+7:y-17;
            goods_picture(d,u->item,x+17,top,1);
        } else if (u->item==CLONE) {
            actor(d,p,HAPPY_POSE,u->frame,85,104,4);
            actor(d,p,HAPPY_POSE,u->frame,177,104,4);
        } else {
            actor(d,p,HAPPY_POSE,u->frame,170,105,4);
            goods_picture(d,u->item,104,106+3*(u->frame%2),2);
            if (u->amount) value_line(d,153,"Won",u->amount);
        }
        break;
    case DISH: {
        static const char *const meals[]={"SCONE","SUSHI","BREAD","CEREAL"};
        static const char *const snacks[]={"CONE","PUDDING","TART","APPLE","APPLEPIE"};
        food_symbol(d,u->snack_menu,144,69,4);
        unsigned id=u->choice>=4?owned_item(p,u->choice-4,true,u->snack_menu):GOODS_COUNT;
        centered(d,132,id<GOODS_COUNT?goods_names[id]:u->choice>=4?"Empty":u->snack_menu?snacks[u->choice]:meals[u->choice],4); break;
    }
    case FOOD:
        food_symbol(d,u->choice!=0,144,69,4);
        centered(d,132,u->choice?"Snack":"Meal",4); break;
    case TRAIN: centered(d,104,u->choice?"Praise":"Time Out",4); break;
    case GAMES: centered(d,104,games[u->choice],4); break;
    case PLAYING: activity(d,u,p); break;
    case NOTICE:
        centered(d,62,u->message,4);
        if (u->care_scene) {care_picture(d,p,u);break;}
        if (u->refused_meal &&
            sheet_pose(d,p->character,SP_NO,u->frame/4,150,115,4,60,false)) break;
        actor(d,p,u->message[0]=='E'?EAT_POSE:u->message[0]=='N'?SAD_POSE:HAPPY_POSE,u->frame,150,115,5);
        if (u->message[0]=='E') food_symbol(d,u->snack_menu,103+3*(u->frame%3),135,3);
        break;
    }
    if (u->page!=HOME && ui_count(u,p)>1) {
        int count=ui_count(u,p);
        int start=0;
        if (count>12) {start=u->choice-5;if (start<0) start=0;if(start>count-12) start=count-12;count=12;}
        int left=(LCD_COLS-(count*12-4))/2;
        for (int i=0;i<count;++i) block(d,left+12*i,183,8,i+start==u->choice?5:2);
    }
}

void draw_display_test(Display *d,unsigned page,unsigned frame)
{
    *d=(Display){0};
#ifdef MOCHI_LAYOUT_TEST
    memset(text_cells,0,sizeof text_cells);
#endif
    switch (page) {
    case 0:
        write_text(d,12,10,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",&menu,1);
        write_text(d,12,36,"ABCDEFGHIJKLM",&menu,3);
        write_text(d,12,68,"NOPQRSTUVWXYZ",&menu,3);
        write_text(d,12,106,"abcdefghijklm",&menu,3);
        write_text(d,12,138,"nopqrstuvwxyz",&menu,3);
        write_text(d,12,177,"0123456789",&menu,3);
        break;
    case 1:
        write_text(d,12,16,"5x7",&menu,2);
        write_text(d,68,12,"0O1Il Aa8",&compact,3);
        write_text(d,12,67,"6x8",&menu,2);
        write_text(d,68,62,"0O1Il Aa8",&menu,3);
        write_text(d,12,118,"8x8",&menu,2);
        write_text(d,68,113,"0O1Il Aa8",&wide,3);
        write_text(d,12,177,"6x8 1x",&menu,1);
        write_text(d,90,177,"2x",&menu,2);
        write_text(d,156,177,"3x",&menu,3);
        write_text(d,222,177,"4x",&menu,4);
        break;
    case 2:
        write_text(d,12,16,"!\"#$%&'()*+,-./",&menu,2);
        write_text(d,12,53,":;<=>?@[\\]^_`{|}~",&menu,2);
        block(d,12,98,144,100);
        for (int y=0;y<8;++y)
            for (int x=0;x<12;++x)
                if ((x+y)%2==0) block(d,172+x*10,98+y*12,10,12);
        break;
    default:
        icon_strip(d,(frame/8)%10,false);
        centered(d,58,icon_names[(frame/8)%10],3);
        for (unsigned i=0;i<7;++i) {
            static const unsigned forms[]={V3_EGG,V3_TELETCHI_BOY,V3_TELETCHI_GIRL,V3_TAMATCHI,
                                           V3_MIZUTAMATCHI,V3_KUCHITAMATCHI,V3_MOHITAMATCHI};
            Pet sample={.character=forms[i],.egg_seconds=(frame/4)%60};
            actor(d,&sample,IDLE_POSE,frame,5+45*i,116+3*((frame/4)%2),3);
        }
        return;
    }
    write_text(d,12,218,"Wheel: page MENU: back",&menu,2);
}


