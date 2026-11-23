#include "characters.h"

const Character v3_characters[V3_COUNT]={
    [V3_LEGACY]={.name="Legacy"},
    [V3_EGG]={.name="Egg",.art=EGG_ART},
#define V3_CHAR(id,key,name,stage,sex,gen,art,rule) [id]={name,CLASS_##stage,sex,gen,art,rule},
#include "v3-roster.inc"
#undef V3_CHAR
};

