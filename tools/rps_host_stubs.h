#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
enum { BTN_ID_CC, BTN_ID_CW, BTN_ID_SP, BTN_ID_LP, BTN_ID_BACK };
enum { HAPTIC_BACK, HAPTIC_TICK, HAPTIC_REVEAL, S_LAYER_OUT };
static struct { bool pressed; uint8_t id; } btn;
static struct { uint8_t index, parent_index[20], layer, state; } ui;
static int u8g2;
static const char *u8g2_font_5x7_tf = "";
int64_t esp_timer_get_time(void);
uint32_t esp_random(void);
const char *nvs_config_get_device_name(void);
bool ble_radio_get_my_mac(uint8_t *mac);
bool ble_radio_is_ready(void);
bool message_service_send_control(uint8_t type, const uint8_t *dst, const void *data, uint8_t len);
void haptic_play(int effect);
void u8g2_SetDrawColor(void *display, int color);
void u8g2_SetFont(void *display, const void *font);
void u8g2_DrawStr(void *display, int x, int y, const char *text);
void u8g2_DrawBox(void *,int,int,int,int);
void u8g2_DrawFrame(void *,int,int,int,int);
#define ui_zh_draw_fit(d,x,y,s,w) ((void)(w),u8g2_DrawStr(d,x,y,s))
#define ui_zh_three_line(d,x,y,s,w,n) ((void)(n),ui_zh_draw_fit(d,x,y,s,w))
#define ui_language_chinese() false
#define ui_zh_lookup(s) (s)
