#ifndef MOCHI_GAME_H
#define MOCHI_GAME_H
#include <stdint.h>
#include <stdbool.h>
#include "characters.h"

#define HATCH_SECONDS 60u
#define REVEAL_BABY 13u

enum Stage { EGG, BABY, CHILD, TEEN, ADULT, DEAD };
enum Care { MEAL, SNACK, FLUSH, MEDICINE, PRAISE, TIME_OUT, LIGHT };
enum Call { QUIET, CRYING, NAUGHTY };
enum Key { KEY_A, KEY_B, KEY_C };
#define MAX_CLOCK 52595999u /* local minutes since 2000, through 2099 */
#define WEEK 10080u

enum Goods {
#define V3_GOOD(id,name,kind,price,retail) id,
#include "goods.inc"
#undef V3_GOOD
    GOODS_COUNT
};
enum GoodKind { MEAL_ITEM,SNACK_ITEM,TOY,SUPPLY,TICKET,COSTUME };
typedef struct {
    uint32_t shelf_seed, secrets, souvenirs, donated, instalments, worn;
    uint8_t bag[GOODS_COUNT], visited; /* first four quantities stay in legacy fields */
} Commerce;
#define LINEAGE_LIMIT 8
enum FamilyPhase { SINGLE, VISIT, WITH_BABY, HANDOVER };
typedef struct {
    uint32_t name, form, mate, history, birth, sex, method, child_sex;
} Ancestor;
typedef struct {
    uint32_t phase, mate, child_sex, birth, event_age, count, child_doses;
    Ancestor records[LINEAGE_LIMIT]; /* newest raising parent first */
} Family;
typedef struct {
    uint32_t seen[2], raised[V3_COUNT]; /* low 16 = first documented generation, high 16 = count */
} Journal;
typedef struct {
    uint32_t last, minutes, awake, hunger, happy, weight, mess, illness;
    uint32_t training, mistakes, neglected, call, dark, stage, points, seed;
    uint32_t pies, sold, route;
    uint32_t setup, birthday_month, birthday_day, user_name, pet_name, sex, paused, muted;
    uint32_t balls, drums, tickets;
    uint32_t physical, mental, hungry_wait, happy_wait, sick_wait, call_wait, light_wait;
    uint32_t stage_start, grade, history, stage_physical, stage_mental;
    uint32_t character, generation, egg_sample, egg_seconds;
    uint32_t clock_hold;
    Family family;
    Commerce shop;
    uint32_t memorial_count, memorial[3][3]; /* name, identity, generation */
    uint32_t sleep_ack, potty_used, last_call, hungry_minutes, ill_minutes;
    Journal journal;
    uint32_t sound_level, snacks_today, cavities, stage_sickness;
} Pet;
void journal_meet(Pet *p,unsigned id);
void journal_raise(Pet *p,unsigned id,unsigned generation);
void journal_import(Pet *p);
const char *item_gate(const Pet *p, enum Goods id);
typedef struct {unsigned wake,bed;} SleepHours;
enum Routine { ROAM, YAWN, BRUSH, BATH, ROLL, SING, DANCE, RUN, SIGNAL, WATCH, SNOWMAN, BUNNY, PUMPKIN, TREE, CAKE, SCHOOL, FEAST, NEW_YEAR, SANTA_VISIT, CLOSE_UP };
SleepHours pet_hours(const Pet *p);
enum Routine pet_routine(const Pet *p);
bool pet_needs_toilet(const Pet *p);
bool is_senior(const Pet *p);
bool family_valid(const Pet *p);
void family_minute(Pet *p);
bool family_answer(Pet *p,bool accept);
bool family_takeover(Pet *p);
bool family_elder_offer(Pet *p);
void pet_clock(Pet *p,uint32_t minute,unsigned second);
void pet_new(Pet *p, uint32_t clock);
void pet_advance(Pet *p, uint32_t clock);
bool pet_sleeping(const Pet *p);
bool pet_attention(const Pet *p);
unsigned pet_happy_max(const Pet *p);
bool pet_care(Pet *p, enum Care care);
uint32_t pet_random(Pet *p);
const char *pet_character(const Pet *p);
void civil_date(uint32_t minute,unsigned *year,unsigned *month,unsigned *day);
int month_days(int year,int month);
char name_letter(uint32_t code,unsigned index);
void setup_adjust(Pet *p,int delta);
void setup_key(Pet *p,enum Key key);
void pet_restart(Pet *p);

extern const char *const goods_names[GOODS_COUNT];
extern const unsigned goods_prices[GOODS_COUNT];
unsigned goods_owned(const Pet *p,enum Goods item);
unsigned shelf_price(const Pet *p,enum Goods id);
bool buy_goods(Pet *p,enum Goods item);
enum Catch { NO_CATCH, POINT_BAG, SMALL_FISH, TIN_CAN, BIG_FISH };
bool use_item_result(Pet *p,enum Goods item,enum Catch *caught);
bool use_item(Pet *p,enum Goods item);
extern const uint8_t goods_kind[GOODS_COUNT];
extern const char *const souvenir_names[32];
unsigned shelf_item(const Pet *p,unsigned slot);
unsigned owned_item(const Pet *p,unsigned ordinal,bool food,bool snack);
unsigned owned_count(const Pet *p,bool food,bool snack);
int code_item(const char *code,bool numeric);
bool town_receipt(const Pet *p,unsigned slot,char digits[11]);
int town_password(const Pet *p,const char *digits);
unsigned code_reward(const Pet *p,int code);
unsigned code_price(const Pet *p,int code);
bool redeem_code(Pet *p,int code);
bool donate(Pet *p,unsigned amount);
bool shop_valid(const Pet *p);
bool eat_pie(Pet *p);
bool pet_valid(const Pet *p);
const char *pet_stage(const Pet *p);
uint32_t civil_minute(int year, int month, int day, int hour, int minute);
#endif
