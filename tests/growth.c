/* Reach forms using the same clocks, care actions and PRNG as the plugin. */
#include "../src/core/growth.h"
#include "../src/storage/save.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static Pet named_baby(unsigned seed,unsigned generation,unsigned grade)
{
    Pet p;
    pet_new(&p,600);
    p.seed=seed;
    p.generation=generation;
    p.grade=grade;
    for (int i=0;i<7;++i) setup_key(&p,KEY_B);
    pet_clock(&p,600,0);
    pet_clock(&p,601,0);
    assert(p.setup==REVEAL_BABY);
    setup_key(&p,KEY_B);
    setup_adjust(&p,1);
    for (int i=0;i<5;++i) setup_key(&p,KEY_B);
    return p;
}

static void care(Pet *p,unsigned physical,unsigned mental)
{
    if (pet_sleeping(p)) {
        if (!p->dark) pet_care(p,LIGHT);
        return;
    }
    if (p->mess) pet_care(p,FLUSH);
    while (p->illness) pet_care(p,MEDICINE);
    if (p->call) pet_care(p,p->call==CRYING?PRAISE:TIME_OUT);
    if (p->hunger<4 && (p->stage_physical>=physical || p->hungry_wait==15)) pet_care(p,MEAL);
    if (p->happy<4 && (p->stage_mental>=mental || p->happy_wait==15)) pet_care(p,SNACK);
}

static void raise_stage(Pet *p,unsigned physical,unsigned mental)
{
    unsigned stage=p->stage;
    do {
        care(p,physical,mental);
        pet_advance(p,p->last+1);
    } while (p->stage==stage);
    assert(p->stage==stage+1 && pet_valid(p));
}

static Pet restored(const Pet *p)
{
    uint8_t bytes[RECORD_BYTES];
    Pet loaded;
    uint32_t serial;
    save_pack(bytes,p,27);
    assert(save_unpack(bytes,sizeof bytes,&loaded,&serial) && serial==27);
    assert(!memcmp(p,&loaded,sizeof loaded));
    bytes[51]^=1;
    assert(!save_unpack(bytes,sizeof bytes,&loaded,&serial));
    return loaded;
}

static void boundaries(void)
{
    const unsigned parent[5][5]={{0,0,1,1,1},{0,1,1,2,2},{1,1,2,2,3},{1,2,2,3,3},{1,2,3,3,3}};
    for (unsigned i=0;i<5;++i)
        for (unsigned j=0;j<5;++j) assert(parent_grade(i,j)==parent[i][j]);
    for (unsigned g=0;g<4;++g) {
        unsigned limit=g==2?4:3;
        assert(teen_grade(2,g,limit,limit)==(g<3?g+1:g));
        assert(teen_grade(3,g,limit-1,limit)==g);
        assert(teen_grade(2,g,limit,limit-1)==g);
        unsigned split=g<2?3:2;
        assert(adult_group(g,split,split)==STUBBORN);
        assert(adult_group(g,split,0)==FRAIL && adult_group(g,0,split)==NAUGHTY_ADULT);
    }
    assert(adult_group(GRADE_A,1,1)==SERIOUS && adult_group(GRADE_A,2,0)==NORMAL);
    assert(adult_group(GRADE_B,1,0)==NORMAL && adult_group(GRADE_B,0,1)==SERIOUS);
    assert(adult_group(GRADE_C,0,0)==SERIOUS && adult_group(GRADE_C,0,1)==NORMAL);
    assert(adult_group(GRADE_D,0,0)==NORMAL);
}

static void persistence(void)
{
    FILE *f=fopen("tests/fixtures/v6-child.a","rb");
    assert(f);
    uint8_t bytes[180];
    assert(fread(bytes,1,sizeof bytes,f)==sizeof bytes && fgetc(f)==EOF);
    fclose(f);
    Pet p;
    uint32_t serial;
    assert(save_unpack(bytes,sizeof bytes,&p,&serial) && serial==11);
    assert(p.stage==CHILD && p.character==V3_TAMATCHI && !history_at(&p,BABY));
    unsigned name=p.pet_name;
    pet_advance(&p,p.last+1);
    assert(p.stage==TEEN && history_at(&p,CHILD)==V3_TAMATCHI);
    Pet copy=restored(&p);
    raise_stage(&p,3,0);raise_stage(&copy,3,0);
    assert(!memcmp(&copy,&p,sizeof p) && p.pet_name==name);
    copy=restored(&p);
    pet_advance(&copy,p.last);pet_advance(&copy,p.last-80);
    assert(!memcmp(&copy,&p,sizeof p));
    p=named_baby(8,1,0);
    while (p.minutes<60) { care(&p,0,0);pet_advance(&p,p.last+1); }
    copy=restored(&p);
    pet_advance(&p,p.last+1441);
    for (unsigned n=0;n<1441;++n) pet_advance(&copy,copy.last+1);
    assert(p.stage==TEEN && history_at(&p,CHILD) && !memcmp(&copy,&p,sizeof p));
}

static void decision_logs(void)
{
    for (unsigned gen=1;gen<=4;++gen) {
        Pet p=named_baby(2006,gen,gen==1?GRADE_A:GRADE_B);
        for (unsigned stage=BABY;stage<=TEEN;++stage) {
            static const unsigned durations[]={0,60,1440,4320};
            p.minutes=p.stage_start+durations[stage]-1;
            GrowthDecision decision;
            Pet before=p;
            assert(!evolve_with_trace(&p,&decision) && !memcmp(&before,&p,sizeof p));
            ++p.minutes;
            p.stage_physical=gen-1;p.stage_mental=gen/2;
            Pet copy=p;
            evolve_pet(&copy);
            assert(evolve_with_trace(&p,&decision) && !memcmp(&p,&copy,sizeof p));
            assert(decision.draw==((decision.seed*1664525u+1013904223u)>>8));
            static const uint8_t expected[4][3]={
                {V3_TAMATCHI,V3_OBOTCHI,V3_MAMETCHI},
                {V3_MOHITAMATCHI,V3_HINATCHI,V3_HIDATCHI},
                {V3_MOHITAMATCHI,V3_OBOTCHI,V3_KUCHIPATCHI},
                {V3_MOHITAMATCHI,V3_HINATCHI,V3_ANDROTCHI}
            };
            assert(decision.selected==expected[gen-1][stage-BABY]);
            assert(decision.selected==decision.pool[decision.draw%decision.count]);
            printf("GROW gen=%u from=%u grade=%u->%u physical=%u mental=%u seed=%u draw=%u pool=",
                   gen,decision.from,decision.grade_before,decision.grade_after,decision.physical,
                   decision.mental,decision.seed,decision.draw);
            for(unsigned k=0;k<decision.count;++k) printf("%s%u",k?",":"",decision.pool[k]);
            printf(" selected=%u %s\n",decision.selected,pet_character(&p));
        }
    }
}

int main(int argc,char **argv)
{
    decision_logs();
    boundaries();
    persistence();
    unsigned seen[2][V3_COUNT]={{0}},total=0;
    const unsigned adult_needs[][2]={{0,0},{2,0},{0,4},{4,0},{4,4}};
    const unsigned toddler_needs[]={0,2,3,4};
    for (unsigned seed=0;seed<128;++seed) {
        for (unsigned generation=1;generation<=2;++generation)
            for (unsigned condition=0;condition<4;++condition) {
                unsigned grade=parent_grade(condition==0?SERIOUS:NORMAL,condition);
                if (generation==2 && condition>=2) grade=parent_grade(condition,condition);
                Pet start=named_baby(seed,generation,generation==1?0:grade);
                raise_stage(&start,0,0);
                assert((start.character>=V3_KUCHITAMATCHI)==(generation>1 && grade<GRADE_C));
                unsigned misses=generation==1?toddler_needs[condition]:0;
                raise_stage(&start,misses,0);
                assert(start.grade==(generation==1?condition:grade));
                for (unsigned a=0;a<5;++a) {
                    Pet p=start;
                    raise_stage(&p,adult_needs[a][0],adult_needs[a][1]);
                    assert(p.grade==adult_group(start.grade,adult_needs[a][0],adult_needs[a][1]));
                    assert(p.character>=18+(generation-1)*15+p.grade*3 &&
                           p.character<21+(generation-1)*15+p.grade*3);
                    assert(p.minutes==5821 && p.pet_name==1 && p.character==history_at(&p,ADULT));
                    for (unsigned stage=BABY;stage<=ADULT;++stage)
                        seen[generation-1][history_at(&p,stage)]|=1u<<p.sex;
                    ++total;
                }
            }
        unsigned done=0;
        for (unsigned id=2;id<=47;++id)
            done+=((seen[0][id]|seen[1][id])==(id<4?1u<<(id-2):3u));
        if (done==46) break;
    }
    unsigned first=0,both=0;
    for (unsigned id=2;id<=47;++id) {
        first+=seen[0][id]!=0;
        if (argc>1 && !strcmp(argv[1],"--coverage"))
            printf("%u,%s,%u,%u\n",id,v3_characters[id].name,seen[0][id],seen[1][id]);
        unsigned sexes=seen[0][id]|seen[1][id];
        assert(sexes==(id<4?1u<<(id-2):3u));
        both+=sexes!=0;
    }
    assert(first==24 && both==46);
    printf("PASS %u egg-to-adult routes, %u first-generation forms, %u across two generations, both eligible sexes; v6/current save replay and multi-stage offline growth\n",total,first,both);
}
