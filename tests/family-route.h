/* Shared host / Rockbox SDL replay. Only elapsed time and player inputs are driven.
   No stage, character, grade, family record, money or needs are injected. */
static void tend_family(Pet *p)
{
    if (pet_sleeping(p)) { if (!p->dark) pet_care(p,LIGHT); return; }
    if (p->hunger<3) pet_care(p,MEAL);
    if (p->happy<3) pet_care(p,SNACK);
    if (p->mess) pet_care(p,FLUSH);
    if (p->illness || p->family.child_doses) pet_care(p,MEDICINE);
    if (p->call) pet_care(p,p->call==CRYING?PRAISE:TIME_OUT);
}

static bool family_route(Pet *p,Ui *u,unsigned generations,bool (*capture)(Pet *,Ui *,const char *))
{
    pet_new(p,civil_minute(2026,11,1,8,0));
    ui_start(u);ui_command(u,p,CMD_OK);
    for (int i=0;i<7;++i) ui_command(u,p,CMD_OK);
    pet_clock(p,p->last,0);pet_clock(p,p->last+1,0);
    for (unsigned gen=1;gen<=generations;++gen) {
        if (p->generation!=gen || p->stage!=BABY || p->setup!=REVEAL_BABY) return false;
        ui_command(u,p,CMD_OK);
        for (unsigned i=0;i<gen;++i) ui_command(u,p,CMD_NEXT);
        for (int i=0;i<5;++i) ui_command(u,p,CMD_OK);
        unsigned start=p->last;
        while (p->stage!=ADULT || (gen<generations && p->family.phase!=VISIT)) {
            tend_family(p);pet_clock(p,p->last+1,0);ui_tick(u,p);
            if (p->stage==DEAD || p->last-start>12*1440) return false;
        }
        if (gen==generations) break;
        if (u->page!=OFFER || !capture(p,u,"offer")) return false;
        ui_command(u,p,CMD_OK);
        if (u->page!=WEDDING || !capture(p,u,"wedding")) return false;
        for (int i=0;i<25;++i) ui_tick(u,p);
        if (!capture(p,u,"birth")) return false;
        while (u->page==WEDDING) ui_tick(u,p);
        while (p->family.phase!=HANDOVER) {
            tend_family(p);pet_clock(p,p->last+1,0);ui_tick(u,p);
            if (p->stage==DEAD || p->last-start>14*1440) return false;
        }
        if (u->page!=DEPARTURE || !capture(p,u,"departure")) return false;
        pet_clock(p,p->last+480,0);
        ui_command(u,p,CMD_OK);
    }
    if (p->family.count!=(generations>LINEAGE_LIMIT?LINEAGE_LIMIT:generations-1) || p->stage!=ADULT) return false;
    while (u->icon!=IC_FRIENDS) ui_command(u,p,CMD_NEXT);
    ui_command(u,p,CMD_OK);ui_command(u,p,CMD_OK);
    return u->page==LINEAGE && capture(p,u,"history");
}
