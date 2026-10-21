#include "quick_msg.h"
#include "esp_log.h"
#include "message_service.h"
#include "nvs_config.h"
#include <string.h>
void quick_msg_init(void) {
    ESP_LOGI("quick_msg", "v3 actual text, asynchronous send");
}
const char *quick_msg_get(uint8_t index) {
    return nvs_config_get_quick_msg(index);
}
bool quick_msg_send_by_id(uint8_t index, const uint8_t *dst) {
    if (index >= QUICK_MESSAGE_COUNT || !dst)
        return false;
    const char *text = quick_msg_get(index);
    if (!text || !*text)
        return false;
    return message_service_send_text(MESSAGE_KIND_QUICK, dst, 0, text, (uint16_t)strlen(text),
                                     !mesh_packet_is_broadcast(dst), NULL);
}