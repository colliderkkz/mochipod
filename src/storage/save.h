#ifndef MOCHI_SAVE_H
#define MOCHI_SAVE_H
#include "../core/game.h"
#define V9_BYTES (204+4*(6+8*LINEAGE_LIMIT+6+10)+GOODS_COUNT+1)
#define V10_BYTES (V9_BYTES+20)
#define V11_BYTES (V10_BYTES+4*(V3_COUNT+6))
#define RECORD_BYTES (V11_BYTES+4)
void save_pack(uint8_t out[RECORD_BYTES], const Pet *p, uint32_t generation);
bool save_unpack(const uint8_t *in, unsigned size, Pet *p, uint32_t *generation);
bool save_newer(uint32_t a, uint32_t b);
#endif
