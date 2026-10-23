#ifndef HALFSTEP_UI_ZH_H
#define HALFSTEP_UI_ZH_H
#include "u8g2.h"
/* Canonical ASCII keys remain unchanged; translate only explicitly curated UI text. */
const char *ui_zh_lookup(const char *text);
u8g2_uint_t ui_zh_width(u8g2_t *display,const char *text);
void ui_zh_draw(u8g2_t *display,u8g2_int_t x,u8g2_int_t baseline,const char *text);
void ui_zh_draw_fit(u8g2_t *display,u8g2_int_t x,u8g2_int_t baseline,const char *text,unsigned width);
void ui_zh_three_line(u8g2_t *display,int x,int baseline,const char *text,unsigned width,unsigned now_ms);
#endif
