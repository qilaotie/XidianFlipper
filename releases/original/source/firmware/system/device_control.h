#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/** After USB driver, EC06 and oled_mirror_begin. */
void device_control_init(void);
/** Nonblocking; call even while OLED is asleep. No command directly calls UI. */
void device_control_tick(void);
void device_control_set_ui_state(uint16_t page);
void device_control_set_performance(uint32_t frames, uint32_t fps_x10,
                                    uint32_t render_us, uint32_t display_us,
                                    uint8_t power_flags);
#ifdef __cplusplus
}
#endif
#endif
