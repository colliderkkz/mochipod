#include "../src/storage/save.h"
#include "../src/core/ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Pet adult(void)
{
    Pet p;pet_new(&p,720);p.stage=ADULT;p.setup=0;p.generation=0;
    p.character=V3_LEGACY;p.points=9999;
    return p;
}

static void check_secrets(void)
{
    static const char *const letters[]={"AABBCACB","ACBBBACC","CBACCABC","CACABABC","BCBCCABA","BCABACBC","ABBAACBA"};
    for (int mode=0;mode<2;++mode) {
        Pet p=adult();Ui u;
        for (int i=0;i<7;++i) {
            ui_start(&u);u.page=CLERK;u.mode=mode;
            ui_command(&u,&p,CMD_NEXT);
            u.frame+=16; /* late tap must start a fresh sequence */
            for (int k=0;k<3;++k) ui_command(&u,&p,CMD_NEXT);
            assert(u.page==CLERK);
            u.frame+=15;
            ui_command(&u,&p,CMD_NEXT);
            assert(u.page==SHOP_CODE);
            for (const char *s=letters[i];*s;++s) {
                int key=*s-'A';
                if (mode==CLASSIC_V3) ui_command(&u,&p,((enum Command[]){CMD_LEFT,CMD_OK,CMD_RIGHT})[key]);
                else {
                    for(int k=0;k<key;++k) ui_command(&u,&p,CMD_NEXT);
                    ui_command(&u,&p,CMD_OK);
                }
            }
            assert(u.page==CLAIM && u.code==i && !code_price(&p,i));
            ui_command(&u,&p,CMD_OK);
        }
        assert(p.shop.secrets==127 && p.points==9999 && goods_owned(&p,OWL_COSTUME)==1);
        assert(!goods_owned(&p,WHOLE_CAKE) && redeem_code(&p,6) && goods_owned(&p,WHOLE_CAKE)==1);
        assert(p.points==9999-goods_prices[WHOLE_CAKE]);
        assert(use_item(&p,CAT_COSTUME) && p.shop.worn==CAT_COSTUME && !goods_owned(&p,CAT_COSTUME));
        assert(code_item("ABC",false)<0 && code_item("6353286367",true)==7 && code_item("3247991490",true)==8);
        p.points=0;Pet before=p;assert(!redeem_code(&p,0) && !memcmp(&p,&before,sizeof p));
        p.balls=30;assert(!redeem_code(&p,7));
    }
}

static void travel_and_donations(void)
{
    static const enum Goods destinations[]={SWITZERLAND,HAWAII,AUSTRALIA,CHINA,CHILE};
    Pet p=adult();
    for(unsigned i=0;i<5;++i) {
        enum Goods ticket=destinations[i];
        bool purchased=false;
        for(unsigned seed=1;seed<5000 && !purchased;++seed) {
            p.shop.shelf_seed=seed;p.sold=0;p.points=9999;
            purchased=buy_goods(&p,ticket);
        }
        assert(purchased && !buy_goods(&p,ticket) && goods_owned(&p,ticket)==1);
        p.stage=TEEN;assert(!use_item(&p,ticket));p.stage=ADULT;
        char receipt[11];
        assert(!town_receipt(&p,9+i,receipt));
        assert(use_item(&p,ticket) && !use_item(&p,ticket) && !(p.shop.souvenirs&(1u<<(9+i))));
        assert(town_receipt(&p,9+i,receipt));
        assert(redeem_code(&p,town_password(&p,receipt)) && (p.shop.souvenirs&(1u<<(9+i))));
    }
    const unsigned payments[]={2000,2000,1000,2000,2000,3000,6000,6000,6000};
    for(unsigned i=0;i<9;++i) {
        p.points=9999;assert(donate(&p,payments[i]) && p.points==9999-payments[i]);
        if (i%3==2) {
            unsigned tier=i/3;char digits[11];
            assert(town_receipt(&p,14+tier,digits));
            assert(!(p.shop.souvenirs&(1u<<(14+tier))) && !donate(&p,1));
            uint8_t bytes[RECORD_BYTES];Pet reload;uint32_t seq;
            save_pack(bytes,&p,7);assert(save_unpack(bytes,sizeof bytes,&reload,&seq));
            p=reload;int reward=town_password(&p,digits);
            if (tier==2) {
                unsigned old=p.balls;p.balls=30-p.shop.bag[ROYAL_COSTUME];
                assert(!redeem_code(&p,reward) && town_receipt(&p,16,digits));
                p.balls=old;
            }
            assert(redeem_code(&p,reward) && redeem_code(&p,reward));
        }
        assert(((p.shop.souvenirs>>14)&7u)==((1u<<((i+1)/3))-1u));
    }
    assert(goods_owned(&p,ROYAL_COSTUME)==1 && !donate(&p,10));
    uint8_t bytes[RECORD_BYTES];Pet back;uint32_t serial;
    save_pack(bytes,&p,88);assert(save_unpack(bytes,sizeof bytes,&back,&serial) && serial==88 && !memcmp(&p,&back,sizeof p));
    for(unsigned t=0;t<3;++t) {
        static const unsigned refresh[]={1440,900,1140};
        p=adult();p.last=refresh[t]-1;p.sold=15;
        pet_advance(&p,p.last+1);assert(!p.sold && p.shop.shelf_seed);
        for(unsigned x=0;x<4;++x) for(unsigned y=x+1;y<4;++y) assert(shelf_item(&p,x)!=shelf_item(&p,y));
    }
}

static void town_vectors(void)
{
    /* A refusal from either food list leaves the meal and its inventory intact. */
    {
        Pet child=adult();child.character=V3_TAMATCHI;child.stage=CHILD;
        child.hunger=1;child.call=NAUGHTY;child.shop.bag[OMELETTE]=1;
        Ui menu;ui_start(&menu);menu.page=DISH;menu.choice=4;
        ui_command(&menu,&child,CMD_OK);
        assert(menu.page==NOTICE && menu.refused_meal && child.shop.bag[OMELETTE]==1 && child.hunger==1);
        ui_command(&menu,&child,CMD_BACK);
        assert(pet_care(&child,TIME_OUT));
        ui_command(&menu,&child,CMD_OK);
        assert(!menu.refused_meal && child.shop.bag[OMELETTE]==0 && child.hunger==2);
    }
    /* Independently generated by MameMame Labs' V3 implementation for AAAAA. */
    Pet p=adult();
    /* Outputs of Shroomtchi's generator, original V3, flag zero. */
    static const char *const expected[2][10]={
        {"0954129585","8998940783","0211609033","5917209366","4394007018","0304407352","2326807688","0109118385","4749118551","4307516545"},
        {"8167429853","0257040717","3748308798","5770709134","4247506786","6269907122","8292307458","9962818152","6260418657","3697216646"}};
    for(unsigned name=0;name<2;++name) {
        Pet desk=p;desk.family.count=2;desk.shop.visited=31;
        if (name) desk.user_name=11+20*26+2*676+10*17576+24*456976; /* LUCKY */
        for(unsigned slot=7;slot<=16;++slot) {
            if (slot>=14) {
                desk.shop.donated=((const unsigned[]){5000,12000,30000})[slot-14];
                desk.shop.instalments=3;
            }
            char digits[11];assert(town_receipt(&desk,slot,digits));
            assert(!strcmp(digits,expected[name][slot-7]));
            Ui ui;ui_start(&ui);ui.page=PASSWORD;
            for(unsigned d=0;d<10;++d) {
                for(int step=0;step<digits[d]-'0';++step) ui_command(&ui,&desk,CMD_NEXT);
                ui_command(&ui,&desk,CMD_OK);
            }
            assert(ui.page==CLAIM);ui_command(&ui,&desk,CMD_OK);
            assert(desk.shop.souvenirs&(1u<<slot));
        }
    }
    int code=town_password(&p,"2433268556");
    assert(code>=0 && code_reward(&p,code)==PIE && redeem_code(&p,code));
    assert(p.points==9849 && p.pies==1);
    assert(town_password(&p,"2433268557")<0 && town_password(&p,"123")<0);
    p.user_name=1;assert(town_password(&p,"2433268556")<0);p.user_name=0;
    assert(redeem_code(&p,town_password(&p,"8628456140")) && (p.shop.souvenirs&1));
    assert(town_password(&p,"3303979396")<0 && town_password(&p,"0446368533")<0);
    p.family.count=1;assert(redeem_code(&p,town_password(&p,"3303979396")));
    assert(town_password(&p,"0446368533")<0);p.family.count=2;
    assert(redeem_code(&p,town_password(&p,"0446368533")) && redeem_code(&p,town_password(&p,"7060779942")));
    assert(goods_owned(&p,PLANT)==1);
    assert(town_password(&p,"3185458509")<0 && town_password(&p,"5213744123")<0);
    p.tickets=1;assert(use_item(&p,HAWAII));assert(town_password(&p,"3185458509")>=0);
    p.shop.visited=0; assert(town_password(&p,"3185458509")<0);
    assert(donate(&p,1000) && donate(&p,2000) && donate(&p,2000));
    assert(town_password(&p,"5213744123")>=0);
    /* Both format flags, encoded independently of the plugin. */
    static const char *const collection[32]={
        "8570241789", "0567645967", "3962242125", "6948446973",
        "9559042460", "1556446638", "4951042796", "3303979396",
        "8998940783", "2049458844", "5917209366", "9003054147",
        "0304407352", "0619054815", "0109118385", "4525744958",
        "4307516545", "7431643282", "0826239441", "3812444289",
        "6423039776", "8420443953", "1815040112", "4801244960",
        "3866237763", "8999644624", "9258238098", "5380445631",
        "4855038434", "9988445295", "0247038770", "3460451671",
    };
    p.shop.visited=31;
    p.shop.souvenirs=(7u<<14);
    for (unsigned i=0;i<32;++i) {
        int pass=town_password(&p,collection[i]);
        assert(pass>=0 && code_reward(&p,pass)==GOODS_COUNT+i && redeem_code(&p,pass));
    }
    assert(p.shop.souvenirs==UINT32_MAX);

}

/* Give one item to exercise the normal use path independently of shelf RNG. */
static void game_weight_chart(void)
{
    /* Explicit chart rows, independent of runtime roster ordering logic. */
    static const unsigned floors[]={
        5,5, 10,10,10,10, 20,20,20,15,15,20,20,20,15,15,
        30,30,30,30,30,30,30,30,30,20,20,20,20,20,20,
        30,30,30,30,30,30,30,30,30,20,20,20,20,20,20,30,30,20
    };
    for (unsigned i=0;i<sizeof floors/sizeof floors[0];++i) {
        Pet p=adult();p.character=V3_TELETCHI_BOY+i;
        p.stage=p.character<=V3_TELETCHI_GIRL?BABY:p.character<=V3_MOHITAMATCHI?CHILD:
                p.character<=V3_HASHITAMATCHI?TEEN:ADULT;
        assert(play_min_weight(&p)==floors[i]);
        for (unsigned extra=2;extra<=3;++extra) {
            Ui u;ui_start(&u);u.page=GAMES;p.weight=floors[i]+extra;
            ui_command(&u,&p,CMD_OK);
            assert(u.page==(extra==2?NOTICE:PLAYING) && p.weight==floors[i]+extra);
        }
    }
    puts("PASS game entry at floor+2/+3 for 49 living forms; weight unchanged");
}

static void drawing_and_disc(void)
{
    Pet p=adult();
    p.shop.bag[PEN]=p.shop.bag[PENCIL]=p.shop.bag[MUSIC_DISC]=2;
    assert(!use_item(&p,MUSIC_DISC) && goods_owned(&p,MUSIC_DISC)==2);
    p.shop.bag[BOOM_BOX]=1;
    p.stage=TEEN;
    assert(!use_item(&p,PENCIL) && goods_owned(&p,PENCIL)==2);
    assert(!use_item(&p,MUSIC_DISC) && goods_owned(&p,BOOM_BOX)==1);
    p.stage=ADULT;
    assert(use_item(&p,PEN) && goods_owned(&p,PEN)==2);
    assert(use_item(&p,PENCIL) && goods_owned(&p,PENCIL)==1);
    assert(use_item(&p,MUSIC_DISC) && !goods_owned(&p,BOOM_BOX));
    assert(!use_item(&p,MUSIC_DISC) && goods_owned(&p,MUSIC_DISC)==1);
}
static void every_item(void)
{
    for (unsigned id=0;id<GOODS_COUNT;++id) {
        Pet p=adult();
        if (id==BALL) p.balls=1;
        else if (id==PIE) p.pies=1;
        else if (id==DRUM) p.drums=1;
        else if (id==HAWAII) p.tickets=1;
        else p.shop.bag[id]=1;
        if (id==MAKE_UP) p.shop.bag[MIRROR]=1;
        if (id==MUSIC_DISC) p.shop.bag[BOOM_BOX]=1;
        assert(use_item(&p,id)==(id!=LOVE_POTION));
        bool consumed=goods_kind[id]<=SNACK_ITEM || goods_kind[id]==TICKET ||
                      (goods_kind[id]==SUPPLY && id!=LOVE_POTION) || id==CAT_COSTUME;
        if (id==FISHING_POLE) assert(goods_owned(&p,id)<=1);
        else assert(goods_owned(&p,id)==(unsigned)!consumed);
        uint8_t data[RECORD_BYTES];Pet back;uint32_t seq;
        save_pack(data,&p,2);
        assert(save_unpack(data,sizeof data,&back,&seq) && seq==2 && !memcmp(&p,&back,sizeof p));
    }
}

static void catches(void)
{
    unsigned mask=0;
    for (unsigned seed=0;seed<128;++seed) {
        Pet p=adult();p.seed=seed;p.points=9900;p.shop.bag[FISHING_POLE]=1;
        Ui u;ui_start(&u);u.page=ITEMS;ui_command(&u,&p,CMD_OK);
        assert(u.page==USING_ITEM && u.item==FISHING_POLE);
        mask|=1u<<u.caught;
        bool retained=u.caught==TIN_CAN || u.caught==SMALL_FISH;
        assert(goods_owned(&p,FISHING_POLE)==(unsigned)retained);
        assert(p.points==(u.caught==POINT_BAG?9999u:9900u));
        assert(u.amount==(u.caught==POINT_BAG?99u:0u));
        if(retained) assert(use_item(&p,FISHING_POLE));
    }
    assert(mask==30);
}

int main(void)
{
    catches();
    Pet sale=adult();
    for(int year=2000;year<2100;++year) {
        sale.last=civil_minute(year,3,1,15,0);
        assert(shelf_price(&sale,FISHING_POLE)==200);
    }
    Pet p=adult();
    p.last=civil_minute(2006,3,1,15,0);
    assert(shelf_price(&p,BALL)==100 && buy_goods(&p,BALL) && p.points==9899);
    p.shop.shelf_seed=13;p.last=civil_minute(2026,12,25,15,0);
    assert(shelf_item(&p,0)==TURKEY);
    p.last=civil_minute(2026,2,14,15,0);assert(shelf_item(&p,0)==CHOCOLATE_HEART);
    p.last=civil_minute(2026,3,12,15,0);assert(shelf_item(&p,0)==COOKIE);
    p.last=civil_minute(2026,5,14,15,0);assert(shelf_item(&p,0)==NOODLES);
    p.birthday_day=14;p.birthday_month=6;p.last=civil_minute(2026,6,14,15,0);
    assert(shelf_item(&p,0)==YOGURT);
    p.character=V3_MAMETCHI;p.happy=1;p.shop.bag[OMELETTE]=1;
    assert(use_item(&p,OMELETTE) && p.happy==5);p.shop.bag[BANANAS]=1;
    assert(use_item(&p,BANANAS) && !p.happy);
    p.stage=TEEN;
    assert(!strcmp(item_gate(&p,PENCIL),"Adult item") && !strcmp(item_gate(&p,CHILE),"Adult item"));
    p.stage=ADULT;assert(!item_gate(&p,PENCIL) && !item_gate(&p,CHILE));
    Ui gate;ui_start(&gate);gate.page=GAMES;p.weight=32;ui_command(&gate,&p,CMD_OK);
    assert(gate.page==NOTICE);gate.page=GAMES;p.weight=33;ui_command(&gate,&p,CMD_OK);
    assert(gate.page==PLAYING);
    game_weight_chart();drawing_and_disc();every_item();check_secrets();travel_and_donations();town_vectors();
    puts("PASS 95 item-use and storage round trips, two code-entry modes, seven letters codes, two universal passwords, named V3 password vectors, ticket purchase and use x5, three royal tiers, shelf and save");
}
