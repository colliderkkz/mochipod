#include "growth.h"

/* V3 column only. A/B and C/D are variants, not different teen sprites. */
static const uint8_t children[2][2]={
    {V3_KUCHITAMATCHI,V3_MOHITAMATCHI}, {V3_MIZUTAMATCHI,V3_TAMATCHI}
};
static const uint8_t teens[2][2][3]={
    {{V3_OBOTCHI,V3_PATAPATATCHI,V3_YOUNG_MAMETCHI},{V3_NIKATCHI,V3_PIRORIROTCHI,0}},
    {{V3_HINATCHI,V3_HIKOTCHI,V3_YOUNG_MIMITCHI},{V3_HINOTAMATCHI,V3_HASHITAMATCHI,0}}
};
static const uint8_t adults[2][5][3]={
    {{V3_MAMETCHI,V3_LEAFTCHI,V3_PYONKOTCHI},{V3_KUCHIPATCHI,V3_MEMETCHI,V3_BILLOTCHI},
     {V3_TARAKOTCHI,V3_PAPARATCHI,V3_MIMIYORITCHI},{V3_HANATCHI,V3_HASHIZOUTCHI,V3_TSUNOTCHI},
     {V3_MASKTCHI,V3_MEGATCHI,V3_KUROKOTCHI}},
    {{V3_MIMITCHI,V3_CHOMAMETCHI,V3_DEKOTCHI},{V3_HIDATCHI,V3_DEBATCHI,V3_BUNBUNTCHI},
     {V3_DOROTCHI,V3_BILL,V3_PIPOTCHI},{V3_ANDROTCHI,V3_WOOLTCHI,V3_TEKETCHI},
     {V3_GOZARUTCHI,V3_SEKITORITCHI,V3_WARUSOTCHI}}
};

unsigned parent_grade(enum AdultGroup first,enum AdultGroup second)
{
    static const uint8_t grades[5][5]={
        {0,0,1,1,1},{0,1,1,2,2},{1,1,2,2,3},{1,2,2,3,3},{1,2,3,3,3}
    };
    return grades[first][second];
}

unsigned teen_grade(unsigned generation,unsigned grade,unsigned physical,unsigned mental)
{
    if (generation==1) {
        unsigned worst=physical>mental?physical:mental;
        if (worst<2) return GRADE_A;
        if (worst==2) return GRADE_B;
        if (worst==3) return GRADE_C;
        return GRADE_D;
    }
    unsigned threshold=grade==GRADE_C?4:3;
    return grade<GRADE_D && physical>=threshold && mental>=threshold ? grade+1 : grade;
}

unsigned adult_group(unsigned grade,unsigned physical,unsigned mental)
{
    unsigned threshold=grade<=GRADE_B?3:2;
    if (physical>=threshold) return mental>=threshold?STUBBORN:FRAIL;
    if (mental>=threshold) return NAUGHTY_ADULT;
    if ((grade==GRADE_A && physical<2 && mental<2) ||
        (grade==GRADE_B && !physical && mental<2) ||
        (grade==GRADE_C && !physical && !mental)) return SERIOUS;
    return NORMAL;
}

/* Four stage-indexed bytes. Zero means that an older save did not record it. */
unsigned history_at(const Pet *p,enum Stage stage)
{
    if (stage<BABY || stage>ADULT) return 0;
    return (p->history>>(8*(stage-BABY)))&255u;
}

void journal_meet(Pet *p,unsigned id)
{
    if (id>=V3_TELETCHI_BOY && id<V3_COUNT)
        p->journal.seen[id/32]|=1u<<(id%32);
}

void journal_raise(Pet *p,unsigned id,unsigned generation)
{
    if (id<V3_TELETCHI_BOY || id>V3_OYAJITCHI || !generation || generation>9999) return;
    journal_meet(p,id);
    uint32_t *record=&p->journal.raised[id];
    if (!*record) *record=generation;
    if ((*record>>16)<65535) *record+=1u<<16;
}

/* Read the surviving stages of ONE pet, including its senior form if present. */
static void import_pet(Pet *p,unsigned history,unsigned current,unsigned generation)
{
    bool included=false;
    for (unsigned slot=0;slot<4;++slot) {
        unsigned id=(history>>(8*slot))&255;
        if (id<V3_TELETCHI_BOY || id>V3_OYAJITCHI ||
            v3_characters[id].life_class!=slot) continue;
        journal_raise(p,id,generation);
        included|=id==current;
    }
    if (!included) journal_raise(p,current,generation);
}

void journal_import(Pet *p)
{
    for (unsigned i=p->memorial_count;i>0;--i)
        journal_raise(p,p->memorial[i-1][1],p->memorial[i-1][2]);
    for (unsigned i=p->family.count;i>0;--i) {
        const Ancestor *a=&p->family.records[i-1];
        import_pet(p,a->history,a->form,p->generation-i);
        journal_meet(p,a->mate);
    }
    import_pet(p,p->history,p->character,p->generation);
    if (p->shop.worn==CAT_COSTUME || p->shop.worn==OWL_COSTUME)
        journal_meet(p,p->shop.worn==CAT_COSTUME?V3_NYATCHI:V3_HOHOTCHI);
    if (p->family.phase>=WITH_BABY) {
        journal_meet(p,p->family.mate);
        journal_meet(p,p->family.child_sex?V3_TELETCHI_GIRL:V3_TELETCHI_BOY);
    }
}

void record_form(Pet *p)
{
    journal_raise(p,p->character,p->generation);
    p->stage_sickness=0;
    unsigned shift=8*(p->stage-BABY);
    p->history=(p->history & ~(255u<<shift)) | (p->character<<shift);
    p->stage_start=p->minutes;
    p->stage_physical=0;
    p->stage_mental=0;
}

bool evolve_with_trace(Pet *p,GrowthDecision *trace)
{
    static const unsigned duration[]={0,60,1440,4320};
    static const uint8_t elder_son[]={V3_OYAJITCHI};
    if (p->stage<BABY || p->stage>TEEN || p->minutes-p->stage_start<duration[p->stage]) return false;
    unsigned old_grade=p->grade, next_stage=p->stage+1;
    unsigned even=p->generation%2==0, count;
    const uint8_t *pool;
    if (p->stage==BABY && p->family.count && p->family.records[0].method==2) {
        pool=elder_son;count=1;next_stage=ADULT;p->grade=NORMAL;
    } else if (p->stage==BABY) {
        pool=children[p->generation==1 || p->grade>=GRADE_C];count=2;
    } else if (p->stage==CHILD) {
        p->grade=teen_grade(p->generation,p->grade,p->stage_physical,p->stage_mental);
        pool=teens[even][p->grade>=GRADE_C];count=p->grade>=GRADE_C?2:3;
    } else {
        p->grade=adult_group(p->grade,p->stage_physical,p->stage_mental);
        pool=adults[even][p->grade];count=3;
    }
    uint32_t seed=p->seed,draw=pet_random(p);
    unsigned selected=pool[draw%count];
    if (trace)
        *trace=(GrowthDecision){p->character,p->generation,old_grade,p->grade,p->stage_physical,
                                p->stage_mental,seed,draw,pool,count,selected};
    p->character=selected;p->stage=next_stage;
    record_form(p);
    return true;
}

void evolve_pet(Pet *p)
{
    evolve_with_trace(p,0);
}

bool growth_valid(const Pet *p)
{
    if (p->stage_start>p->minutes || p->stage_physical>9999 || p->stage_mental>9999) return false;
    if (p->character==V3_LEGACY)
        return !(p->history || p->grade || p->stage_start || p->stage_physical || p->stage_mental);
    if (!p->generation || p->generation>9999 || p->grade>STUBBORN) return false;
    if (p->character==V3_EGG)
        return !p->history && !p->stage_start && p->grade<=GRADE_D;
    const Character *current=&v3_characters[p->character];
    bool elder=is_senior(p),special=p->character==V3_OYAJITCHI;
    if (elder && p->character!=(p->sex?V3_OTOKITCHI:V3_OJITCHI)) return false;
    if (special && (p->sex || !p->family.count || p->family.records[0].method!=2)) return false;
    if (!elder && !special && current->rule>ADULT_CARE) return false;
    unsigned living_stage=elder?ADULT:current->life_class+BABY;
    if ((p->stage!=DEAD && p->stage!=living_stage) ||
        (living_stage<ADULT && p->grade>GRADE_D) ||
        (!elder && history_at(p,(enum Stage)living_stage)!=p->character) ||
        (elder && !history_at(p,ADULT))) return false;
    for (unsigned stage=BABY;stage<=ADULT;++stage) {
        unsigned id=history_at(p,(enum Stage)stage);
        if (!id) continue;
        if (id<V3_TELETCHI_BOY || id>=V3_COUNT || stage>living_stage) return false;
        const Character *c=&v3_characters[id];
        if ((c->rule>ADULT_CARE && !(id==V3_OYAJITCHI && !p->sex && p->family.count && p->family.records[0].method==2)) || c->life_class+1u!=stage ||
            (c->sex!=EITHER && c->sex!=p->sex) ||
            (c->generation!=ANY && c->generation!=(p->generation%2?ODD:EVEN)) ||
            (c->rule==PARENT_CHILD && p->generation==1)) return false;
    }
    return true;
}
