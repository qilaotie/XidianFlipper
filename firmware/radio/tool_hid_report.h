#ifndef TOOL_HID_REPORT_H
#define TOOL_HID_REPORT_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* Keep the on-air report contract unchanged: report 1 keyboard, report 2
 * 16-bit Consumer Control. GATT report values do not prepend report IDs. */
static inline bool tool_hid_encode(bool media,uint16_t usage,uint8_t out[8]) {
    if(!usage || usage>(media?0x3ff:0x65))return false;
    memset(out,0,8);
    if(media){out[0]=(uint8_t)usage;out[1]=(uint8_t)(usage>>8);}
    else out[2]=(uint8_t)usage;
    return true;
}
static inline bool tool_hid_pulse_valid(uint16_t milliseconds) {
    return milliseconds>=30 && milliseconds<=250;
}
#endif
