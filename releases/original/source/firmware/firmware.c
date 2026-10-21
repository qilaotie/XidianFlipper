/**
 * @file    firmware.c
 * @brief   顶层固件初始化 — V6.0 WouoUI 移植
 *
 * firmware_init_all() 按正确顺序编排各子系统启动。
 * 在冷启动或完全唤醒时由 app_main() 调用。
 */

#include "firmware.h"
#include "menu_content.h"
#include "assets.h"
#include "nvs_config.h"
#include "load_switch.h"
#include "rgb_led.h"
#include "esp_log.h"
#include "esp_app_desc.h"
#include "u8g2_esp32_hal.h"
#include "oled_mirror.h"
#include "device_control.h"
#include "message_service.h"
#include "morse_playback.h"
#include "morse_training_ui.h"
#include "local_pages.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "fw_init";

/* ── OLED Mirror: USB CDC 发送回调 ─────────────────────────
 * 短超时(10ms): USB 未连接时静默丢弃，不阻塞 UI 渲染 */
static bool usb_cdc_send_callback(const uint8_t *data, size_t len)
{
    if (!usb_serial_jtag_is_driver_installed() || !usb_serial_jtag_is_connected()) return false;
    return usb_serial_jtag_write_bytes((const char *)data, len, pdMS_TO_TICKS(10))==(int)len;
}

#if defined(ENABLE_OLED_MIRROR) && ENABLE_OLED_MIRROR
static int mirror_log_vprintf(const char *format, va_list args)
{
    char line[256];
    int length=vsnprintf(line,sizeof(line),format,args);
    if (length>0) oled_mirror_send_log(line);
    return length;
}
#endif

void firmware_init_all(void)
{
    ESP_LOGI(TAG, "=== BLUE_LONG_MESH %s ===", esp_app_get_description()->version);

    /* 1. 事件系统（需在任何任务通信之前初始化） */
    event_init();

    /* 1.5. 负载开关（GPIO4 集中管理，需在任何外设使用前初始化） */
    load_switch_init();

    /* 2. GPIO 已在 app_main 中由 gpio_init_all() 完成 */

    /* 2.5. 日志管理器 — 必须在事件系统之后、其他模块之前 */
    log_manager_init();

    /* 3. 蜂鸣器 — 初始化但保持停止 */
    buzzer_init();

    /* 4. 触觉反馈系统 — 蜂鸣器之后初始化
     *      创建 FreeRTOS 软件定时器以支持非阻塞音效播放 */
    haptic_init();

    /* 5. NVS 配置（从闪存加载，启用延迟保存）
     *    注意: storage_nvs (WouoUI 参数) 在 ui_init 阶段由 nvs_init() 初始化
     *    V7.3 修复: 移到 rgb_led_init 之前，避免 g_config 零值导致 brightness=0% */
    nvs_config_init();

    /* 6. WS2812 — 初始化 RMT + LED 灯效引擎 */
    rgb_init();
    rgb_led_init();

    /* 7. EC06 统一输入驱动（ISR + FreeRTOS 任务）
     *    ENC_A/B/GPIO2/3, ENC_BTN/GPIO18, KEY/GPIO19 */
    ec06_config_t ec06_cfg = EC06_CONFIG_DEFAULT;
    ec06_init(&ec06_cfg);

    /* 8. 充电检测 */
    charge_status_init();

    /* ── OLED 提前初始化（在 BLE 之前，消除上电黑屏等待） ── */
    nvs_init();                     /* 加载/默认化 ui.param[] + knob.param[] */
    ui_init();                      /* 初始化页面行数/图标数 */
    /* 应用持久化的按键阈值到 ec06（ui.param ×BTN_PARAM_TIMES = ms） */
    ec06_set_press_thresholds((uint32_t)ui.param[BTN_SPT] * BTN_PARAM_TIMES,
                              (uint32_t)ui.param[BTN_LPT] * BTN_PARAM_TIMES);
    ui.index = M_HOME;              /* 启动进入主页 */
    ui.state = S_LAYER_IN;
    ui.layer = 0;
    ui.fade = 1;
    memset(ui.select, 0, sizeof(ui.select));
    list.line_n = DISP_H / LIST_LINE_H;

    oled_power_on();                /* GPIO4 → OLED_3V3, 含 150ms 稳定等待 */

    /* WS2812B 上电后首次复位：发送全黑数据，确保 LED 进入已知状态 */
    rgb_set_color(0, 0, 0);

    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = I2C_SDA;
    hal.bus.i2c.scl = I2C_SCL;
    u8g2_esp32_hal_init(hal);
    u8g2_Setup_ssd1306_i2c_128x32_univision_f(
        &u8g2, U8G2_R0,
        u8g2_esp32_i2c_byte_cb,
        u8g2_esp32_gpio_and_delay_cb);
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
    u8g2_InitDisplay(&u8g2);

    /* OLED Mirror: 安装 USB Serial/JTAG 驱动（TX buffer 需 ≥ 帧大小 517B） */
    {
        usb_serial_jtag_driver_config_t usj_cfg = {
            .tx_buffer_size = 4096,  /* Whole image/log/control frames fit without partial writes. */
            .rx_buffer_size = 256,
        };
        esp_err_t ret = usb_serial_jtag_driver_install(&usj_cfg);
        if (usb_serial_jtag_is_driver_installed()) usb_serial_jtag_vfs_use_driver();
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "USJ driver installed (TXbuf=4096) for OLED Mirror");
        } else {
            ESP_LOGW(TAG, "USJ driver install failed: %s", esp_err_to_name(ret));
        }
    }
    /* OLED Mirror: 绑定 U8g2 + USB CDC 发送回调 */
    oled_mirror_begin(&u8g2, usb_cdc_send_callback);
#if defined(ENABLE_OLED_MIRROR) && ENABLE_OLED_MIRROR
    /* Keep runtime ESP logs inside the same framing/mutex as images and replies. */
    if (usb_serial_jtag_is_driver_installed()) esp_log_set_vprintf(mirror_log_vprintf);
#endif
    device_control_init();

    if (oled_is_available()) {
        oled_buffer_init();

        /* V10.7: 冷启动显示开机图片 2s + 黑屏 1s；
         * 空闲 DeepSleep 按键唤醒时跳过，直接进入 UI。 */
        if (g_wake_reason == WAKE_REASON_COLD_BOOT) {
            /* 上电显示开机图片 (LSB 格式, u8g2_DrawXBM) */
            u8g2_ClearBuffer(&u8g2);
            u8g2_DrawXBM(&u8g2, 0, 0, 128, 32, splash_img);
            u8g2_SendBuffer(&u8g2);
            oled_mirror_send_frame();
            u8g2_SetPowerSave(&u8g2, 0);
            u8g2_SetContrast(&u8g2, ui.param[DISP_BRI]);
            vTaskDelay(pdMS_TO_TICKS(2000));     /* 开机图片停留 2 秒 */

            /* 黑屏 1 秒 → 平滑过渡到 UI */
            u8g2_ClearBuffer(&u8g2);
            u8g2_SendBuffer(&u8g2);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            /* 热唤醒：直接恢复显示，无动画 */
            u8g2_SetPowerSave(&u8g2, 0);
            u8g2_SetContrast(&u8g2, ui.param[DISP_BRI]);
            u8g2_ClearBuffer(&u8g2);
            u8g2_SendBuffer(&u8g2);
            ESP_LOGI(TAG, "Warm wake — OLED on, no boot animation");
        }
    } else {
        ESP_LOGW(TAG, "OLED not detected — running headless");
    }

    /* 9. 协议层 — 加密必须在无线之前就绪 */
    mesh_crypto_init();

    /* 10. BLE 无线 — 启动 NimBLE 主机任务（高优先级）
     *     V9.0: 改用 mesh_notify_dispatch 作为 RX 回调，直接分发消息。
     *     NimBLE 同步是异步的；可轮询 ble_radio_is_ready() 检查。
     *     注意：gatt_server_init() 在 ble_radio_init() 之后调用，
     *     因为 GATT 服务注册要求 NimBLE 主机已初始化
     *     （nimble_port_init），但主机任务
     *     （nimble_port_freertos_init）已在此处启动。 */
    ble_radio_init(mesh_notify_dispatch, NULL);

    /* V6.45: 检查 BLE 致命错误 — MAC 读取失败则弹窗提示并停止 */
    {
        uint8_t fatal_err = ble_radio_get_fatal_error();
        if (fatal_err != 0) {
            ESP_LOGE(TAG, "BLE fatal error 0x%02X — displaying on OLED", fatal_err);
            /* 红色 LED 快闪 10 次 */
            for (int i = 0; i < 10; i++) {
                rgb_set_color(255, 0, 0);
                vTaskDelay(pdMS_TO_TICKS(100));
                rgb_off();
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            rgb_set_color(255, 0, 0);  /* 保持红色常亮 */

            /* OLED 显示错误弹窗 */
            u8g2_ClearBuffer(&u8g2);
            u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
            u8g2_SetDrawColor(&u8g2, 1);

            /* 标题框 */
            u8g2_DrawFrame(&u8g2, 4, 0, 120, 32);
            u8g2_DrawHLine(&u8g2, 4, 13, 120);

            /* 错误码 */
            char err_buf[32];
            snprintf(err_buf, sizeof(err_buf), "FATAL ERR 0x%02X", fatal_err);
            u8g2_DrawStr(&u8g2, 16, 11, err_buf);

            /* 说明文字 */
            u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
            if (fatal_err == 0x01) {
                u8g2_DrawStr(&u8g2, 8, 23, "eFuse MAC Read Failed");
            } else if (fatal_err == 0x02) {
                u8g2_DrawStr(&u8g2, 8, 23, "BLE Addr Set Failed");
            }
            u8g2_DrawStr(&u8g2, 8, 31, "System Halted");

            u8g2_SendBuffer(&u8g2);

            /* 死循环 — 仅看门狗可复位 */
            while (1) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }

    /* 11. 快捷消息模块 */
    quick_msg_init();
    quick_msg_menu_refresh();   /* V6.8: 加载 NVS 消息到菜单文字 */

    /* 12. 消息通知分发 */
    mesh_notify_init();

    /* 16. V7.7: 统一莫斯输入引擎 (GPIO18 轮询) + ITU 二叉树 */
    morse_input_init(GPIO_NUM_18);
    morse_tree_init();
    morse_playback_init();
    morse_training_ui_init();

    /* 16b. V9.0: morse_engine 仅保留给后续模块使用，此处不初始化 */

    /* 16d. V7.0: Morse CQ 广播系统 — 接收队列 + UI */
    morse_rx_queue_init();
    morse_ui_init();

    /* 16e. V7.4: 莫斯终端 — 独立输入解码系统（第三磁贴） */
    morse_terminal_init();

    /* 18. 阶段四：配置模式状态机 */
    config_mode_init();

    /* 19. 阶段六：好友管理（碰一碰添加） */
    message_service_init();
    local_pages_init();
    friends_menu_refresh();  /* V6.11: 加载联系人到 UI */
    ui.num[M_FRIENDS] = 1;  /* V6.11: 动态条目数 */

    /* V9.0: 移除默认测试好友 — 不再使用 node_id */

    /* 20. GATT 服务器 — V10.3: gatt_server_init 未实装，暂时禁用 */
    /* gatt_server_init(NULL, NULL, NULL, NULL); */

    /* 21. 开发者模式（条件编译） */
#if LOG_DEV_MODE_ENABLE
    dev_mode_init();
#endif

    ESP_LOGI(TAG, "All firmware subsystems ready: BLUE_LONG_MESH %s",
             esp_app_get_description()->version);

    /* ── V9.0: 启动后系统状态 dump ──────────────────────── */
    ESP_LOGI(TAG, "=== SYSTEM STATUS ===");
    ESP_LOGI(TAG, "  BLE ready: %d", ble_radio_is_ready());
    ESP_LOGI(TAG, "  Quick msg seq: %lu", (unsigned long)g_config.quick_msg_seq);
    ESP_LOGI(TAG, "  Contacts: %u", g_config.contact_count);
    ESP_LOGI(TAG, "======================");

    /* V6.19: 首次开机引导 — 若设备名为空, 自检后强制进入命名页 */
    {
        const char *dev_name = nvs_config_get_device_name();
        if (!dev_name || dev_name[0] == '\0') {
            ESP_LOGI(TAG, "First boot detected — running self-test");

            /* 自检: RGB 白闪一次 + 蜂鸣器短响 (仅首次开机) */
            rgb_set_color(255, 255, 255);
            vTaskDelay(pdMS_TO_TICKS(150));
            rgb_off();
            buzzer_beep(80);

            ESP_LOGI(TAG, "Entering first-boot naming guide");
            ui.index = M_NAME_SETUP;
            ui.state = S_LAYER_IN;
            ui.layer = 0;
            ui.select[0] = 0;
        }
    }
}
