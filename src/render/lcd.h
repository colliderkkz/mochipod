#ifndef MOCHI_LCD_H
#define MOCHI_LCD_H
#include "../core/ui.h"

/* Rockbox monochrome bitmap layout: eight vertical pixels per byte, LSB at top. */
#define LCD_COLS 320
#define LCD_ROWS 240
typedef struct { uint8_t bits[LCD_ROWS/8][LCD_COLS]; } Display;
void draw_frame(Display *d,const Pet *p,const Ui *u);
void draw_display_test(Display *d,unsigned page,unsigned frame);
#endif
