#include "../src/core/ui.h"
#include "../src/storage/save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void round_trip(Pet *p)
{
    uint8_t bytes[RECORD_BYTES];
    uint32_t generation;
    Pet loaded;
    assert(pet_valid(p));
    save_pack(bytes,p,31);
    assert(save_unpack(bytes,sizeof bytes,&loaded,&generation));
    assert(generation==31 && !memcmp(p,&loaded,sizeof loaded));
    *p=loaded;
}

static unsigned hatch(unsigned seed)
{
    Pet p;
    pet_new(&p,600);
    p.seed=seed;
    for (int i=0;i<7;++i) setup_key(&p,KEY_B);
    assert(p.character==V3_EGG && !p.setup && !pet_care(&p,MEAL));
    pet_clock(&p,600,59);
    pet_clock(&p,601,0);
    assert(p.egg_seconds==1);
    pet_clock(&p,601,55);
    assert(p.egg_seconds==56);
    round_trip(&p);
    pet_clock(&p,600,0);
    assert(p.egg_seconds==56);
    p.paused=1;
    pet_clock(&p,700,0);
    p.paused=0;
    pet_clock(&p,700,3);
    assert(p.egg_seconds==59);
    pet_clock(&p,700,4);
    assert(p.stage==BABY && p.setup==REVEAL_BABY && p.generation==1);
    assert(p.character==(p.sex?V3_TELETCHI_GIRL:V3_TELETCHI_BOY));
    unsigned result=1u<<p.sex;
    round_trip(&p);
    pet_clock(&p,700+WEEK,4);
    assert(p.minutes==1 && p.stage==BABY && !pet_care(&p,SNACK));
    setup_key(&p,KEY_B);
    assert(p.setup==8);
    setup_adjust(&p,1);
    for (int i=0;i<5;++i) setup_key(&p,KEY_B);
    assert(!p.setup && name_letter(p.pet_name,0)=='B');
    round_trip(&p);
    uint32_t last=p.last;
    pet_advance(&p,last+30);
    assert(pet_sleeping(&p) && !pet_care(&p,MEAL));
    pet_advance(&p,last+35);
    assert(!pet_sleeping(&p));
    pet_advance(&p,last+59);
    assert(p.stage==BABY);
    pet_advance(&p,last+60);
    assert(p.stage==CHILD && (p.character==V3_MIZUTAMATCHI || p.character==V3_TAMATCHI));
    round_trip(&p);
    return result | (p.character==V3_MIZUTAMATCHI?4u:8u);
}

static void new_game_confirmation(void)
{
    Pet p;
    pet_new(&p,200);
    p.generation=0; p.character=V3_LEGACY; p.setup=0; p.stage=ADULT;
    p.points=300; p.pet_name=12; p.user_name=15;
    Pet original=p;
    Ui u;
    ui_start(&u);
    ui_command(&u,&p,CMD_OK);
    ui_command(&u,&p,CMD_BACK);
    ui_command(&u,&p,CMD_PREV);
    ui_command(&u,&p,CMD_OK);
    assert(u.page==HATCH_ASK);
    ui_command(&u,&p,CMD_OK);
    assert(u.page==REPLACE_ASK);
    ui_command(&u,&p,CMD_BACK);
    assert(u.page==HATCH_ASK && !memcmp(&original,&p,sizeof p));
    ui_command(&u,&p,CMD_OK);
    assert(u.page==REPLACE_ASK);
    ui_command(&u,&p,CMD_OK);
    assert(p.stage==EGG && p.generation==1 && p.character==V3_EGG);
    assert(p.user_name==original.user_name && p.points==0 && p.pet_name==0);
}

static void migrate_file(const char *path)
{
    FILE *f=fopen(path,"rb");
    assert(f);
    uint8_t old[164],fresh[RECORD_BYTES];
    assert(fread(old,1,sizeof old,f)==sizeof old && fgetc(f)==EOF);
    fclose(f);
    Pet p;
    uint32_t serial;
    assert(save_unpack(old,sizeof old,&p,&serial));
    assert(p.character==V3_LEGACY && !p.generation);
    save_pack(fresh,&p,serial);
    /* Every old payload byte, including the route, names, clocks and inventory. */
    assert(!memcmp(old+8,fresh+8,152));
    round_trip(&p);
}

int main(int argc,char **argv)
{
    unsigned observed=0;
    for (unsigned seed=0;seed<128;++seed) observed|=hatch(seed);
    assert(observed==15);
    new_game_confirmation();
    for (int i=1;i<argc;++i) migrate_file(argv[i]);
    printf("PASS hatch boundary, pause, restore, both sexes, two first-generation children, double confirmation, %d v5 files\n",argc-1);
    return 0;
}
