#include "game.h"

#define V3_GOOD(id,name,kind,price,retail) name,
const char *const goods_names[GOODS_COUNT]={
#include "goods.inc"
};
#undef V3_GOOD
#define V3_GOOD(id,name,kind,price,retail) price,
const unsigned goods_prices[GOODS_COUNT]={
#include "goods.inc"
};
#undef V3_GOOD
#define V3_GOOD(id,name,kind,price,retail) kind,
const uint8_t goods_kind[GOODS_COUNT]={
#include "goods.inc"
};
#undef V3_GOOD
#define V3_GOOD(id,name,kind,price,retail) retail,
static const bool on_sale[GOODS_COUNT]={
#include "goods.inc"
};
#undef V3_GOOD
const char *const souvenir_names[32]={
#include "souvenirs.inc"
};

unsigned goods_owned(const Pet *p,enum Goods id)
{
    switch (id) {
    case PIE:return p->pies;
    case BALL:return p->balls;
    case DRUM:return p->drums;
    case HAWAII:return p->tickets;
    default:return (unsigned)id<GOODS_COUNT?p->shop.bag[id]:0;
    }
}

static void change_quantity(Pet *p,unsigned id,int delta)
{
    switch (id) {
    case PIE:p->pies+=delta;break;
    case BALL:p->balls+=delta;break;
    case DRUM:p->drums+=delta;break;
    case HAWAII:p->tickets+=delta;break;
    default:p->shop.bag[id]+=delta;break;
    }
}

static bool is_food(unsigned id) { return goods_kind[id]<=SNACK_ITEM; }

static unsigned quantity(const Pet *p,bool food)
{
    unsigned total=0;
    for (unsigned i=0;i<GOODS_COUNT;++i)
        if (is_food(i)==food) total+=goods_owned(p,i);
    return total;
}

unsigned owned_item(const Pet *p,unsigned ordinal,bool food,bool snack)
{
    for (unsigned i=0;i<GOODS_COUNT;++i) {
        if (!goods_owned(p,i) || food!=is_food(i) || (food && (goods_kind[i]==SNACK_ITEM)!=snack)) continue;
        if (!ordinal--) return i;
    }
    return GOODS_COUNT;
}

unsigned owned_count(const Pet *p,bool food,bool snack)
{
    unsigned total=0;
    for (unsigned i=0;i<GOODS_COUNT;++i)
        if (goods_owned(p,i) && is_food(i)==food && (!food || (goods_kind[i]==SNACK_ITEM)==snack)) ++total;
    return total;
}

/* A persisted restock minute determines four different offers. Shelf draws
   are independent of growth RNG and can be reconstructed after a restart. */
unsigned shelf_item(const Pet *p,unsigned slot)
{
    if (slot>=4) return GOODS_COUNT;
    if (!p->shop.shelf_seed) return slot;
    uint32_t rng=p->shop.shelf_seed;
    unsigned chosen[4];
    unsigned year,month,day,seasonal=GOODS_COUNT;
    civil_date(p->last,&year,&month,&day);
    if (month==12 && day>=23 && day<=25) seasonal=(day!=25 && (rng&2))?WHOLE_CAKE:TURKEY;
    else if ((month==2 || month==11) && day==14) seasonal=CHOCOLATE_HEART;
    else if ((month==3 && day==12) || (month==10 && day==14)) seasonal=COOKIE;
    else if ((month==3 || month==5) && day==14) seasonal=NOODLES;
    else if (month==p->birthday_month && day==p->birthday_day) seasonal=YOGURT;
    for (unsigned n=0;n<=slot;++n) {
        rng=rng*1664525u+1013904223u;
        unsigned id=(rng>>8)%GOODS_COUNT;
        if (!n && seasonal!=GOODS_COUNT) {chosen[n]=seasonal;continue;}
        for (;;) {
            unsigned j=0;
            while (j<n && chosen[j]!=id) ++j;
            if (on_sale[id] && j==n && id!=TURKEY && id!=WHOLE_CAKE && id!=NOODLES && id!=YOGURT && id!=CHOCOLATE_HEART && id!=COOKIE) break;
            id=(id+1)%GOODS_COUNT;
        }
        chosen[n]=id;
    }
    return chosen[slot];
}

static bool acquire(Pet *p,unsigned id,unsigned price)
{
    if (id>=GOODS_COUNT || p->stage==DEAD || p->stage==EGG || pet_sleeping(p) ||
        p->points<price || quantity(p,is_food(id))>=(is_food(id)?15u:30u)) return false;
    p->points-=price;
    change_quantity(p,id,1);
    return true;
}

unsigned shelf_price(const Pet *p,enum Goods id)
{
    /* 2006 owners report weekly half-price sales, including March 1.
       A fixed weekly day is the offline approximation of the undocumented schedule. */
    if ((unsigned)id>=GOODS_COUNT) return 0;
    unsigned year,month,day;
    civil_date(p->last,&year,&month,&day);
    bool sale=(month==3 && day==1) || p->last/1440%7==4;
    return sale?goods_prices[id]/2:goods_prices[id];
}

bool buy_goods(Pet *p,enum Goods id)
{
    for (unsigned slot=0;slot<4;++slot) {
        if (shelf_item(p,slot)!=(unsigned)id) continue;
        unsigned mask=1u<<slot;
        if ((p->sold&mask) || !acquire(p,id,shelf_price(p,id))) return false;
        p->sold|=mask;return true;
    }
    return false;
}

static const struct {const char *text;unsigned id;} codes[]={
    {"AABBCACB",RC_CAR_2},{"ACBBBACC",HAIR_GEL},{"CBACCABC",CUCKOO_CLOCK},
    {"CACABABC",CAT_COSTUME},{"BCBCCABA",LOVE_POTION},{"BCABACBC",STEAK},
    {"ABBAACBA",WHOLE_CAKE},{"6353286367",PEN},{"3247991490",CLONE}
};

int code_item(const char *text,bool numeric)
{
    for (int i=numeric?7:0;i<(numeric?9:7);++i) {
        const char *a=text,*b=codes[i].text;
        while (*a && *a==*b) {++a;++b;}
        if (*a==*b) return i;
    }
    return -1;
}

/* Player enters upper five digits, then lower five. Original V3 format
   documented by Shroomtchi (MameMame Labs). No web service is called. */
enum { TOWN_ITEM=9, TOWN_SOUVENIR=TOWN_ITEM+GOODS_COUNT, TOWN_GIFT=TOWN_SOUVENIR+32 };
static uint32_t town_user_key(const Pet *p)
{
    uint32_t user=0;
    for (unsigned i=0;i<5;++i) {
        unsigned ch=name_letter(p->user_name,i)-'A'+1;
        if (ch>10) ch+=2;
        user|=ch<<(5*i);
    }
    return user;
}

/* The local desk issues website-to-toy reward codes. The original site also
   accepted toy-to-site visit codes, which are a different format. */
static const unsigned king_levels[]={5000,12000,30000};

static unsigned next_king(const Pet *p)
{
    unsigned tier=0;
    while (tier<3 && (p->shop.souvenirs&(1u<<(14+tier)))) ++tier;
    return tier;
}

static bool king_ready(const Pet *p,unsigned tier)
{
    return tier<3 && tier==next_king(p) && p->shop.instalments>=3 &&
           p->shop.donated>=king_levels[tier];
}

bool town_receipt(const Pet *p,unsigned slot,char digits[11])
{
    unsigned type,marker;
    if (slot==7 || slot==8) {
        if (p->family.count<slot-6) return false;
        type=130;marker=slot==7?1:65;
    } else if (slot>=9 && slot<=13 && (p->shop.visited&(1u<<(slot-9)))) {
        type=131;marker=slot+120;
    } else if (slot>=14 && slot<=16 && king_ready(p,slot-14)) {
        static const unsigned markers[]={195,197,200};
        type=132;marker=markers[slot-14];
    } else return false;
    unsigned id=128+slot;
    uint32_t checksum=(0u-type-id-marker)&255;
    uint32_t packed=((checksum<<24)|(marker<<16)|(id<<8)|type)^town_user_key(p)^0x46464646u;
    /* The toy displays low five decimal digits above the high five. */
    for (int i=0;i<10;++i) {
        digits[i<5?4-i:14-i]='0'+packed%10;
        packed/=10;
    }
    digits[10]=0;
    return true;
}

int town_password(const Pet *p,const char *digits)
{
    unsigned halves[2]={0,0};
    for (unsigned i=0;i<10;++i) {
        if (digits[i]<'0' || digits[i]>'9') return -1;
        halves[i/5]=halves[i/5]*10+(unsigned)(digits[i]-'0');
    }
    if (digits[10]) return -1;
    uint64_t packed=(uint64_t)halves[1]*100000u+halves[0];
    unsigned flag=packed>>32;
    if (flag>1) return -1;
    uint32_t plain=(uint32_t)packed^town_user_key(p)^(flag?0x2c2c2c2cu:0x46464646u);
    unsigned type=plain&255,id=(plain>>8)&255,arg=(plain>>16)&255;
    if ((type+id+arg+(plain>>24)+flag)&255) return -1;
    if (type==130) {
        bool grand=(arg&64)!=0;
        if ((arg&128) || p->family.count<(grand?2u:1u)) return -1;
        static const uint8_t ids[2][3]={{135,1,43},{136,75,31}};
        unsigned slot=arg&3;
        if (!slot || ids[grand][slot-1]!=id) return -1;
        if (slot==1) return TOWN_SOUVENIR+id-128;
        return TOWN_GIFT+(grand?(slot==2?PLANT:FISHING_POLE):(slot==2?PENCIL:MUSIC_DISC));
    }
    if (id>=128 && id<160) {
        unsigned bit=id-128;
        if (id==135 || id==136) return -1;
        if (id>=137 && id<=144) {
            unsigned marker=id<=141?id-8:id==142?195:id==143?197:200;
            if (type!=(id<=141?131u:132u) || arg!=marker) return -1;
            if (id<=141 && !(p->shop.visited&(1u<<(id-137)))) return -1;
            if (id>=142 && !(p->shop.souvenirs&(1u<<bit)) && !king_ready(p,id-142)) return -1;
        } else if (type!=129) return -1;
        return TOWN_SOUVENIR+bit;
    }
    static const uint8_t mapping[114]={
#include "town-items.inc"
    };
    if (type!=129 || id>=sizeof mapping || !mapping[id]) return -1;
    return TOWN_ITEM+mapping[id]-1;
}

unsigned code_price(const Pet *p,int code)
{
    if (code>=TOWN_ITEM && code<TOWN_SOUVENIR) return goods_prices[code-TOWN_ITEM];
    if ((unsigned)code>=7 || !(p->shop.secrets&(1u<<code))) return 0;
    return goods_prices[codes[code].id];
}

unsigned code_reward(const Pet *p,int code)
{
    if (code>=TOWN_GIFT && code<TOWN_GIFT+GOODS_COUNT) return code-TOWN_GIFT;
    if (code>=TOWN_ITEM && code<TOWN_GIFT) return code-TOWN_ITEM;
    if ((unsigned)code>=9) return GOODS_COUNT;
    if (code<7 && (p->shop.secrets|(1u<<code))==127 && p->shop.secrets!=127) return OWL_COSTUME;
    return codes[code].id;
}

bool redeem_code(Pet *p,int code)
{
    if (code>=TOWN_ITEM && code<TOWN_GIFT+GOODS_COUNT) {
        unsigned id=code_reward(p,code);
        if (id<GOODS_COUNT) return acquire(p,id,code_price(p,code));
        unsigned slot=id-GOODS_COUNT;
        if (slot>=14 && slot<=16 && !(p->shop.souvenirs&(1u<<slot))) {
            if (!king_ready(p,slot-14)) return false;
            if (slot==16 && !acquire(p,ROYAL_COSTUME,0)) return false;
            p->shop.instalments=0;
        }
        p->shop.souvenirs|=1u<<slot;
        return true;
    }
    if ((unsigned)code>=9) return false;
    unsigned bit=1u<<code,after=p->shop.secrets|(bit&127u);
    unsigned id=code_reward(p,code);
    if (!acquire(p,id,code_price(p,code))) return false;
    p->shop.secrets=after;return true;
}

const char *item_gate(const Pet *p,enum Goods id)
{
    if ((unsigned)id>=GOODS_COUNT) return "No item";
    if (id==LOVE_POTION) return "Infrared only";
    if (pet_sleeping(p)) return "Asleep";
    if (p->stage==DEAD || p->stage==EGG) return "Unavailable";
    if (p->character!=V3_LEGACY && p->illness) return "Medicine first";
    unsigned earliest=ADULT;
    if (is_food(id)) earliest=BABY;
    switch (id) {
    case CHEST:case SHOVEL:case GRAMOPHONE:case DOLL:case ACTION_FIGURE:case BOOM_BOX:
        earliest=BABY;break;
    case BALL: earliest=CHILD;break;
    case BUILDING_BLOCKS: earliest=TEEN;break;
    default: break;
    }
    if (p->stage<earliest) return earliest==ADULT?"Adult item":earliest==TEEN?"Teen item":"Toddler item";
    if (id==MAKE_UP && !goods_owned(p,MIRROR)) return "Need mirror";
    if (id==MUSIC_DISC && !goods_owned(p,BOOM_BOX)) return "Need boombox";
    return 0;
}

bool use_item_result(Pet *p,enum Goods id,enum Catch *caught)
{
    if (caught) *caught=NO_CATCH;
    if (item_gate(p,id) || !goods_owned(p,id)) return false;
    if (is_food(id)) {
        if (!pet_care(p,goods_kind[id]==MEAL_ITEM?MEAL:SNACK)) return false;
        static const uint8_t tastes[V3_COUNT][2]={
#include "tastes.inc"
        };
        if (id==STEAK || tastes[p->character][0]==(unsigned)id+1) {p->happy=pet_happy_max(p);p->happy_wait=0;}
        else if (tastes[p->character][1]==(unsigned)id+1) p->happy=0;
        change_quantity(p,id,-1);return true;
    }
    if ((id==DOLL && p->sex) || (id==ACTION_FIGURE && !p->sex)) {
        if (p->happy<pet_happy_max(p)) ++p->happy;
        p->happy_wait=0;
    }
    if (goods_kind[id]==TICKET) {
        unsigned souvenir=id==SWITZERLAND?10:id==HAWAII?11:id==AUSTRALIA?12:id==CHINA?13:14;
        p->shop.visited|=1u<<(souvenir-10);
        change_quantity(p,id,-1);
    } else if (goods_kind[id]==COSTUME) {
        p->shop.worn=id;
        if (id==CAT_COSTUME || id==OWL_COSTUME) journal_meet(p,id==CAT_COSTUME?V3_NYATCHI:V3_HOHOTCHI);
        if (id==CAT_COSTUME) change_quantity(p,id,-1);
    } else if (id==FISHING_POLE) {
        /* Keep the four-way RC.2 draw. Catch frequencies and prize distribution
           have not been measured on a region-identified original. */
        enum Catch result=(enum Catch)(POINT_BAG+pet_random(p)%4);
        if (caught) *caught=result;
        if (result==POINT_BAG || result==BIG_FISH) change_quantity(p,id,-1);
        if (result==POINT_BAG) {
            p->points+=500;
            if (p->points>9999) p->points=9999;
        }
    } else if (goods_kind[id]==SUPPLY) {
        change_quantity(p,id,-1);
        if (id==MUSIC_DISC) change_quantity(p,BOOM_BOX,-1);
        if (id==PLANT || id==SHOVEL || id==CHEST || id==LAMP) {
            unsigned outcome=pet_random(p)%4;
            if (outcome==0) {
                p->points+=500;
                if (p->points>9999) p->points=9999;
            } else if (outcome==1 && (id==CHEST || id==LAMP)) {
                change_quantity(p,id==CHEST?DOLL:BOOM_BOX,1);
            } else if (outcome==2 && (id==CHEST || id==LAMP)) {
                p->hunger=p->happy=0;
            }
        }
    }
    return true;
}

bool use_item(Pet *p,enum Goods id) {return use_item_result(p,id,0);}

bool eat_pie(Pet *p) {return use_item(p,PIE);}

bool donate(Pet *p,unsigned amount)
{
    if (!amount || p->points<amount || p->stage==DEAD || p->stage==EGG) return false;

    unsigned tier=next_king(p);
    if (tier==3 || king_ready(p,tier)) return false;
    p->points-=amount;p->shop.donated+=amount;
    if (p->shop.donated>30000) p->shop.donated=30000;
    if (p->shop.instalments<3) ++p->shop.instalments;
    return true;
}

bool shop_valid(const Pet *p)
{
    if (p->shop.visited>31 || p->shop.secrets>127 || p->shop.donated>30000 || p->shop.instalments>30000 ||
        (p->shop.worn && p->shop.worn!=CAT_COSTUME && p->shop.worn!=OWL_COSTUME &&
         p->shop.worn!=ROYAL_COSTUME) || p->memorial_count>3) return false;
    for (unsigned i=0;i<p->memorial_count;++i)
        if (p->memorial[i][0]>=11881376 || p->memorial[i][1]>=V3_COUNT || p->memorial[i][2]>9999) return false;
    return quantity(p,true)<=15 && quantity(p,false)<=30;
}
