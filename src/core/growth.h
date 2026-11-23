#ifndef MOCHI_GROWTH_H
#define MOCHI_GROWTH_H
#include "game.h"

enum CareGrade { GRADE_A, GRADE_B, GRADE_C, GRADE_D };
enum AdultGroup { SERIOUS, NORMAL, NAUGHTY_ADULT, FRAIL, STUBBORN };
unsigned parent_grade(enum AdultGroup first,enum AdultGroup second);
unsigned teen_grade(unsigned generation,unsigned toddler_grade,unsigned physical,unsigned mental);
unsigned adult_group(unsigned teen_grade_value,unsigned physical,unsigned mental);
typedef struct {
    unsigned from,generation,grade_before,grade_after,physical,mental;
    uint32_t seed,draw;
    const uint8_t *pool;
    unsigned count,selected;
} GrowthDecision;
bool evolve_with_trace(Pet *p,GrowthDecision *trace);
void evolve_pet(Pet *p);
unsigned history_at(const Pet *p,enum Stage stage);
void record_form(Pet *p);
bool growth_valid(const Pet *p);
#endif
