/* Development-only test. Link with upstream V3Emu v3cpu.c (see v3-reference.md). */
#include "v3cpu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t image[512*1024], snapshot[8192];
int main(void)
{
    /* A tiny independently authored test program at $C000 turns on the LCD,
       sets its top-left dot in display RAM and loops. */
    const uint8_t program[]={0xa9,1,0x85,0x24,0x8d,0x10,0x3e,0x4c,7,0xc0};
    memcpy(image+0x4000,program,sizeof program);
    image[0x7ffd]=0xc0;
    v3_init(image,sizeof image);
    v3_run(512000);
    int mode=0;
    const uint8_t *lcd=v3_vram(&mode);
    assert(mode==1 && lcd[16]==1 && v3_pc()==0xc007);
    assert(v3_state_size()<=sizeof snapshot);
    v3_state_save(snapshot);
    v3_reset();
    assert(v3_state_load(snapshot,v3_state_size()));
    assert(v3_pc()==0xc007 && v3_vram(&mode)[16]==1);
    puts("PASS synthetic V3 CPU program, LCD RAM and state round trip");
}

