#ifndef MOCHI_CHARACTERS_H
#define MOCHI_CHARACTERS_H
#include <stdint.h>

enum CharacterId {
    V3_LEGACY, V3_EGG,
#define V3_CHAR(id,key,name,stage,sex,gen,art,rule) V3_##key=id,
#include "v3-roster.inc"
#undef V3_CHAR
    V3_COUNT
};
enum ArtId { NO_ART, EGG_ART, BOY_ART, GIRL_ART, MIZU_ART, TAMA_ART, KUCHI_ART, MOHI_ART };
enum LifeClass { CLASS_BABY, CLASS_CHILD, CLASS_TEEN, CLASS_ADULT, CLASS_SENIOR, CLASS_COSTUME, CLASS_DEBUG };
enum SexLimit { MALE, FEMALE, EITHER, UNKNOWN_SEX };
enum GenerationLimit { ANY, ODD, EVEN, NOT_APPLICABLE };
enum Evolution { FROM_EGG, EGG_CHILD, PARENT_CHILD, TEEN_CARE, ADULT_CARE, AGE, ELDER_PARENTS, ITEM_ONLY, DEBUG_ONLY };
typedef struct {
    const char *name;
    uint8_t life_class, sex, generation, art, rule;
} Character;
extern const Character v3_characters[V3_COUNT];

#endif
