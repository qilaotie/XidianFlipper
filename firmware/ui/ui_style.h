#ifndef HALFSTEP_UI_STYLE_H
#define HALFSTEP_UI_STYLE_H
#include <stdbool.h>
enum {UI_SKIN_PIXEL,UI_SKIN_MINIMAL,UI_SKIN_TERMINAL,UI_SKINS};
enum {UI_WIDGET_MESSAGES,UI_WIDGET_PET,UI_WIDGET_SCORES,UI_WIDGET_AUTO,UI_WIDGETS};
unsigned ui_style_skin(void);
unsigned ui_style_widget(void);
bool ui_style_set(unsigned skin,unsigned widget);
const char *ui_style_skin_label(void);
const char *ui_style_widget_label(void);
#endif
