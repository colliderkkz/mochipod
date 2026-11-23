/* Appended to the simulator-only test translation unit. Production builds
   never link this entry point or the accelerated timeline. */
#ifndef SIMULATOR
#error Family replay is for the Rockbox Simulator only
#endif
#include "family-route.h"
static bool replay_frame(Pet *p,Ui *u,const char *scene)
{
    if (!commit_pet(p)) return false;
    Pet loaded;
    uint32_t seq;
    if (!load_slot(paths[active_slot],&loaded,&seq) || serial!=seq ||
        rb->memcmp(p,&loaded,sizeof loaded)) return false;
    *p=loaded;
    paint(u);
    char file[96];
    rb->snprintf(file,sizeof file,"/replay-%lu-%s",(unsigned long)p->generation,scene);
    int fd=rb->open(file,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if (fd<0 || rb->close(fd)<0) return false;
    while (rb->button_get(true)!=BUTTON_SELECT) {}
    return true;
}
enum plugin_status plugin_start(const void *parameter)
{
    (void)parameter;
    Ui ui;
    active_slot=-1;serial=0;
    rb->lcd_set_backdrop(NULL);
    rb->lcd_set_background(LCD_WHITE);rb->lcd_set_foreground(LCD_BLACK);
    rb->lcd_set_drawmode(DRMODE_SOLID);
    return family_route(&pet,&ui,4,replay_frame)?PLUGIN_OK:PLUGIN_ERROR;
}
