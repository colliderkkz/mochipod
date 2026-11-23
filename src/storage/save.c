#include "save.h"
#include "../core/rtc_guard.h"
#include "../core/growth.h"

static void put32(uint8_t *out,uint32_t n)
{
    for (unsigned i=0;i<4;++i) out[i]=(uint8_t)(n>>(8*i));
}
static uint32_t get32(const uint8_t *in)
{
    uint32_t n=0;
    for (unsigned i=0;i<4;++i) n|=(uint32_t)in[i]<<(8*i);
    return n;
}
static uint32_t checksum(const uint8_t *in,unsigned size)
{
    uint32_t crc=UINT32_MAX;
    for (unsigned i=0;i<size;++i) {
        crc^=in[i];
        for (unsigned j=0;j<8;++j) crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
/* Stable field order, independent of C structure layout. */
#define SAVE_FIELDS(X) X(last) X(minutes) X(awake) X(hunger) X(happy) X(weight) X(mess) X(illness) X(training) X(mistakes) X(neglected) X(call) X(dark) X(stage) X(points) X(seed)
void save_pack(uint8_t out[RECORD_BYTES],const Pet *p,uint32_t generation)
{
    put32(out,0x3356504d); /* MPV3 */
    put32(out+4,12);
    put32(out+8,generation);
    unsigned offset=12;
#define PACK(name) put32(out+offset,p->name); offset+=4;
    SAVE_FIELDS(PACK) PACK(pies) PACK(sold) PACK(route)
    PACK(setup) PACK(birthday_month) PACK(birthday_day) PACK(user_name) PACK(pet_name) PACK(sex) PACK(paused) PACK(muted)
    PACK(physical) PACK(mental) PACK(hungry_wait) PACK(happy_wait) PACK(sick_wait) PACK(call_wait) PACK(light_wait)
    PACK(balls) PACK(drums) PACK(tickets)
    PACK(character) PACK(generation) PACK(egg_sample) PACK(egg_seconds)
    PACK(stage_start) PACK(grade) PACK(history) PACK(stage_physical) PACK(stage_mental) PACK(clock_hold)
    PACK(family.phase) PACK(family.mate) PACK(family.child_sex) PACK(family.birth) PACK(family.event_age) PACK(family.count)
    for (unsigned i=0;i<LINEAGE_LIMIT;++i) {
        PACK(family.records[i].name) PACK(family.records[i].form) PACK(family.records[i].mate) PACK(family.records[i].history) PACK(family.records[i].birth) PACK(family.records[i].sex) PACK(family.records[i].method) PACK(family.records[i].child_sex)
    }
    PACK(shop.shelf_seed) PACK(shop.secrets) PACK(shop.souvenirs) PACK(shop.donated) PACK(shop.instalments) PACK(shop.worn)
    for (unsigned i=0;i<sizeof p->shop.bag;++i) out[offset++]=p->shop.bag[i];
    out[offset++]=p->shop.visited;
    PACK(memorial_count)
    for (unsigned i=0;i<3;++i) for (unsigned j=0;j<3;++j) { PACK(memorial[i][j]) }
    PACK(sleep_ack) PACK(potty_used) PACK(last_call) PACK(hungry_minutes) PACK(ill_minutes)
    PACK(journal.seen[0]) PACK(journal.seen[1])
    for (unsigned i=0;i<V3_COUNT;++i) { PACK(journal.raised[i]) }
    PACK(sound_level) PACK(snacks_today) PACK(cavities) PACK(stage_sickness) PACK(family.child_doses)
#undef PACK
    put32(out+offset,checksum(out,offset));
}

bool save_unpack(const uint8_t *in,unsigned size,Pet *p,uint32_t *generation)
{
    if ((size!=80 && size!=92 && size!=124 && size!=152 && size!=164 && size!=180 && size!=200 && size!=204 && size!=V9_BYTES && size!=V10_BYTES && size!=V11_BYTES && size!=RECORD_BYTES) || get32(in)!=0x3356504d ||
        get32(in+4)!=(size==80 ? 1u : size==92 ? 2u : size==124 ? 3u : size==152 ? 4u : size==164 ? 5u : size==180 ? 6u : size==200 ? 7u : size==204 ? 8u : size==V9_BYTES ? 9u : size==V10_BYTES ? 10u : size==V11_BYTES ? 11u : 12u) ||
        get32(in+size-4)!=checksum(in,size-4)) return false;
    Pet temp={.birthday_month=1,.birthday_day=1};
    unsigned offset=12;
#define UNPACK(name) temp.name=get32(in+offset); offset+=4;
    SAVE_FIELDS(UNPACK)
    if (size>=92) { UNPACK(pies) UNPACK(sold) UNPACK(route) }
    if (size>=124) {
        UNPACK(setup) UNPACK(birthday_month) UNPACK(birthday_day) UNPACK(user_name)
        UNPACK(pet_name) UNPACK(sex) UNPACK(paused) UNPACK(muted)
    }
    if (size>=152) {
        UNPACK(physical) UNPACK(mental) UNPACK(hungry_wait) UNPACK(happy_wait)
        UNPACK(sick_wait) UNPACK(call_wait) UNPACK(light_wait)
    }
    if (size>=164) { UNPACK(balls) UNPACK(drums) UNPACK(tickets) }
    if (size>=180) { UNPACK(character) UNPACK(generation) UNPACK(egg_sample) UNPACK(egg_seconds) }
    if (size>=200) {
        UNPACK(stage_start) UNPACK(grade) UNPACK(history) UNPACK(stage_physical) UNPACK(stage_mental)
        if (size>=204) { UNPACK(clock_hold) }
        if (size>=V9_BYTES) {
            UNPACK(family.phase) UNPACK(family.mate) UNPACK(family.child_sex) UNPACK(family.birth) UNPACK(family.event_age) UNPACK(family.count)
            for (unsigned i=0;i<LINEAGE_LIMIT;++i) {
                UNPACK(family.records[i].name) UNPACK(family.records[i].form) UNPACK(family.records[i].mate) UNPACK(family.records[i].history) UNPACK(family.records[i].birth) UNPACK(family.records[i].sex) UNPACK(family.records[i].method) UNPACK(family.records[i].child_sex)
            }
            UNPACK(shop.shelf_seed) UNPACK(shop.secrets) UNPACK(shop.souvenirs) UNPACK(shop.donated) UNPACK(shop.instalments) UNPACK(shop.worn)
            for (unsigned i=0;i<sizeof temp.shop.bag;++i) temp.shop.bag[i]=in[offset++];
            temp.shop.visited=in[offset++];
            UNPACK(memorial_count)
            for (unsigned i=0;i<3;++i) for (unsigned j=0;j<3;++j) { UNPACK(memorial[i][j]) }
        }
    } else if (size==180 && temp.character!=V3_LEGACY) {
        if (temp.generation!=1 || temp.character>V3_TAMATCHI) return false;
        if (temp.character!=V3_EGG) {
            enum Stage form=temp.character<=V3_TELETCHI_GIRL?BABY:CHILD;
            temp.history=temp.character<<(8*(form-BABY));
            temp.stage_start=form==BABY?1:61;
            /* v6 had no per-stage counters. The baby is the only stage whose
               entire care record belongs to the current form. */
            if (form==BABY) { temp.stage_physical=temp.physical; temp.stage_mental=temp.mental; }
        }
    }
    if (size>=V10_BYTES) {
        UNPACK(sleep_ack) UNPACK(potty_used) UNPACK(last_call) UNPACK(hungry_minutes) UNPACK(ill_minutes)
    }
    if (size>=V11_BYTES) {
        UNPACK(journal.seen[0]) UNPACK(journal.seen[1])
        for (unsigned i=0;i<V3_COUNT;++i) {
            UNPACK(journal.raised[i])
            uint32_t entry=temp.journal.raised[i];
            if (entry && (i<V3_TELETCHI_BOY || i>V3_OYAJITCHI || !(entry>>16) ||
                !(entry&65535) || (entry&65535)>9999 ||
                !(temp.journal.seen[i/32]&(1u<<(i%32))))) return false;
        }
        UNPACK(sound_level) UNPACK(snacks_today) UNPACK(cavities) UNPACK(stage_sickness)
        if (temp.sound_level>2 || temp.snacks_today>255 || temp.cavities>1 ||
            (temp.cavities && !temp.illness) || temp.stage_sickness>4) return false;
    }
    if (size==RECORD_BYTES) { UNPACK(family.child_doses) }
#undef UNPACK
    if (size<V10_BYTES && (temp.hunger>4 || temp.happy>4 || temp.mess>4)) return false;
    if (temp.clock_hold>CLOCK_LIVE_JUMP || !pet_valid(&temp)) return false;
    if (size<V11_BYTES) journal_import(&temp);
    *p=temp;
    *generation=get32(in+8);
    return true;
}
bool save_newer(uint32_t a,uint32_t b) { return a!=b && a-b<0x80000000u; }
