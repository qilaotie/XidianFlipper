/**
 * @file    nvs_config.h
 * @brief   基于 NVS 的配置管理 — V5.5 Phase 6
 *
 * 使用 RAM 镜像 + 延迟写入策略管理持久化设置：
 *   - 快捷消息（10 × 48 字节）
 *   - 用户设置（LED/蜂鸣器/振动使能、超时等）
 *   - 联系人别名映射（4字节 ID → 设备名 + 别名）
 *   - 设备名称（通过手机设置，好友碰一碰时交换）
 *
 * 写入策略（§14.五.2/8）：
 *   - GATT/UI 修改仅更新 RAM 镜像
 *   - 5 秒 FreeRTOS 单次定时器消抖写入
 *   - 仅在定时器到期或显式保存时提交到 NVS
 *   - NVS 双扇区冗余，保证断电安全
 */

#ifndef NVS_CONFIG_H
#define NVS_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "defaults.h"
#include "log_config.h"   /* LOG_DEV_MODE_ENABLE 条件编译 */

#ifdef __cplusplus
extern "C" {
#endif

/* ── 联系人同步状态 ─────────────────────────────────────── */

/** 每个联系人条目的同步状态 */
typedef enum {
    CONTACT_SYNC_DONE   = 0,    /**< 已与手机同步 */
    CONTACT_SYNC_PENDING = 1,   /**< 设备端已添加，等待手机同步 */
    CONTACT_SYNC_LOCAL   = 2,   /**< 仅本地存储（未同步到手机） */
} contact_sync_state_t;

/* ── 配置结构体（RAM 镜像）──────────────────────────────── */

/** 快捷消息条目 */
typedef struct {
    char text[QUICK_MESSAGE_MAX_LEN];   /**< 以空字符结尾的消息文本 */
    bool in_use;                         /**< 此槽位是否有数据 */
} quick_msg_entry_t;

/** 联系人别名映射（阶段六扩展） */
typedef struct {
    uint32_t node_id;                   /**< 4 字节 Mesh 节点 ID */
    char     device_name[8];            /**< 好友设备名（碰一碰时交换获取） */
    char     alias[8];                  /**< 自定义别名/备注（通过手机设置，7 字符 + 空字符） */
    uint8_t  sync_state;                /**< 同步状态（参见 contact_sync_state_t） */
    bool     in_use;                    /**< 此槽位是否占用 */
    bool     pinned;                    /**< 是否置顶（V6.32: 好友列表排序优先） */
} contact_alias_t;

/** 最大联系人数量 */
#define NVS_CONTACT_MAX         32

/** 用户配置（RAM 镜像） */
typedef struct {
    /* ── 指示器使能 ──────────────────────────────────────── */
    bool     led_enabled;
    bool     buzzer_enabled;
    bool     vibration_enabled;

    /* ── RGB 灯效配置 (V7.0) ─────────────────────────────── */
    uint8_t  rgb_brightness;               /**< 全局亮度 0-100 */
    uint8_t  rgb_color_menu[3];            /**< 菜单进入灯色 */
    uint8_t  rgb_color_delivered[3];       /**< 送达确认灯色 */
    uint8_t  rgb_color_cached[3];          /**< 未送达缓存灯色 */
    uint8_t  rgb_color_forward[3];         /**< 转发消息灯色 */
    uint8_t  rgb_color_received[3];        /**< 收到消息灯色 */
    uint16_t rgb_flash_duration_ms;        /**< 快闪持续时间 */
    uint16_t rgb_breathing_period_ms;      /**< 呼吸周期 */
    uint16_t rgb_unread_interval_ms;       /**< 未读呼吸间隔 */

    /* ── 显示设置 (V6.13) ────────────────────────────────── */
    bool     dark_mode;                  /**< 黑暗模式 (true=暗底白字) */

    /* ── OLED 超时（秒，0-255）──────────────────────────── */
    uint8_t  oled_anim_timeout;         /**< 空闲 → 动画超时 */
    uint8_t  oled_sleep_timeout;        /**< Legacy NVS field, retained but never used for sleep. */

    /* ── 莫尔斯设置 ──────────────────────────────────────── */
    uint16_t morse_dot_ms;              /**< V7.7: 莫斯点持续时长（ms, 40-300, 默认200） */
    uint32_t morse_letter_gap_ms;       /**< 字母间隔（600-900 毫秒，由dot_ms动态计算） */
    uint32_t morse_auto_send_ms;        /**< 自动发送空闲超时 */
    uint8_t  morse_wpm;                 /**< 码速，词/分钟（教学用，V7.7弃用） */
    bool     morse_auto_decode;         /**< 接收端自动译码开关（V5.8） */

    /* ── 日志/开发者模式设置（V0.10.0，条件编译）───────── */
#if LOG_DEV_MODE_ENABLE
    uint8_t  log_level;                 /**< 运行时日志级别 0-5 */
    bool     usb_log_enabled;           /**< USB 日志输出 */
    bool     mesh_log_enabled;          /**< Mesh 模块日志 */
    bool     gatt_log_enabled;          /**< GATT 模块日志 */
    bool     system_log_enabled;        /**< 系统诊断日志 */
#endif

    /* ── Mesh 设置 ───────────────────────────────────────── */
    uint8_t  mesh_default_ttl;
    uint32_t deepsleep_interval_sec;

    /* Legacy idle-sleep setting: keep its position/size for cfg_blob compatibility. */
    uint16_t idle_sleep_timeout_sec;    /**< Retained NVS value; automatic sleep is unsupported. */

    /* ── 长文本滚动 (V6.13) ──────────────────────────────── */
    bool     long_text_scroll_enable;    /**< 菜单长文本自动滚动开关 */
    uint8_t  long_text_scroll_speed;     /**< 滚动速度 (1-10 像素/帧) */
    uint8_t  long_text_threshold_px;     /**< 触发滚动的宽度阈值 (64-128 px) */

    /* ── 快捷消息 ────────────────────────────────────────── */
    quick_msg_entry_t quick_msgs[QUICK_MESSAGE_COUNT];

    /* ── 快捷消息序列号（V6.39: 单调递增，NVS 持久化） ──── */
    uint32_t          quick_msg_seq;

    /* ── 联系人别名 ──────────────────────────────────────── */
    contact_alias_t   contacts[NVS_CONTACT_MAX];
    uint8_t           contact_count;

    /* ── 设备身份（阶段六）───────────────────────────────── */
    char              my_device_name[8];  /**< 本设备名称（通过手机设置，7 字符 + 空字符） */

    /* ── 脏标记（内部使用）───────────────────────────────── */
    bool     _dirty;                     /**< RAM 已修改，尚未写入 NVS */
} nvs_config_t;

/* ── 全局配置实例 ───────────────────────────────────────── */

/** 所有持久化配置的全局 RAM 镜像。 */
extern nvs_config_t g_config;

/* ── API 接口 ────────────────────────────────────────────── */

/**
 * @brief 初始化 NVS 并从闪存加载配置。
 *
 * 如果 NVS 为空（首次启动），则用默认值填充 g_config。
 * 创建 5 秒写消抖 FreeRTOS 定时器。
 * 必须在 NVS 闪存子系统初始化后调用一次。
 */
void nvs_config_init(void);

/**
 * @brief 运行 NVS 分区完整性检查与恢复。
 *
 * 探测主 NVS 扇区是否损坏。如果主扇区损坏，触发重新初始化
 * （擦除 + 从备份扇区迁移）。由 nvs_config_init() 自动调用。
 */
void nvs_config_recovery_check(void);

/**
 * @brief 从 NVS 加载配置到 RAM 镜像。
 *
 * 通常由 nvs_config_init() 调用。
 * 可再次调用以撤销未保存的更改。
 */
void nvs_config_load(void);

/**
 * @brief 将 RAM 镜像保存到 NVS（立即写入，不消抖）。
 *
 * 通常由消抖定时器处理保存。
 * 在 DeepSleep 前或 BLE 断开连接时显式调用。
 * 单个 writer 保存稳定快照；写入期间的新修改继续保持 _dirty 并延后重试。
 * 已有 writer 时立即返回且保持 _dirty，停机/OTA 调用方必须检查 _dirty。
 */
void nvs_config_save(void);

/**
 * @brief 标记配置已修改（触发延迟保存）。
 *
 * 可从任意任务安全调用。重置 5 秒消抖定时器。
 */
void nvs_config_mark_dirty(void);

/**
 * @brief 按索引获取快捷消息（0..9）。
 *
 * @param index  消息索引。
 * @return       指向空字符结尾字符串的指针，若为空则返回 NULL。
 */
const char *nvs_config_get_quick_msg(uint8_t index);

/**
 * @brief 设置快捷消息（仅 RAM，标记脏）。
 *
 * @param index  消息索引（0..9）。
 * @param text   空字符结尾文本（最大 QUICK_MESSAGE_MAX_LEN-1 字符）。
 * @return       成功返回 true，索引越界返回 false。
 */
bool nvs_config_set_quick_msg(uint8_t index, const char *text);

/**
 * @brief 按节点 ID 查找联系人别名。
 *
 * @param node_id  4 字节 Mesh 节点 ID。
 * @return         别名字符串，未找到返回 NULL。
 */
const char *nvs_config_get_alias(uint32_t node_id);

/**
 * @brief 添加或更新联系人（含设备名和别名，仅 RAM，标记脏）。
 *
 * 阶段六：从 nvs_config_set_alias() 扩展。同时存储好友的设备名
 * （来自碰一碰交换）和可选别名。如果联系人已存在则更新字段，
 * 否则追加。新设备添加的联系人 sync_state 设为 CONTACT_SYNC_PENDING。
 *
 * @param node_id      4 字节 Mesh 节点 ID。
 * @param device_name  好友设备名（最多 7 字符，可为 NULL 保留现有值）。
 * @param alias        自定义别名/备注（最多 7 字符，可为 NULL 保留现有值）。
 * @param sync_state   初始同步状态（设备添加为 CONTACT_SYNC_PENDING，
 *                     手机添加为 CONTACT_SYNC_DONE）。
 * @return             成功返回 true，表满返回 false。
 */
bool nvs_config_set_contact_full(uint32_t node_id, const char *device_name,
                                  const char *alias, uint8_t sync_state);

/**
 * @brief 按节点 ID 删除联系人。
 *
 * 从 RAM 表中移除联系人并标记配置脏。
 *
 * @param node_id  4 字节 Mesh 节点 ID。
 * @return         已删除返回 true，未找到返回 false。
 */
bool nvs_config_delete_alias(uint32_t node_id);

/**
 * @brief 获取联系人的显示名称（别名优先，否则显示设备名）。
 *
 * @param node_id  4 字节 Mesh 节点 ID。
 * @return         显示名称字符串，未找到返回 NULL。
 */
const char *nvs_config_get_contact_display_name(uint32_t node_id);

/**
 * @brief 设置本设备名称（仅 RAM，标记脏）。
 *
 * 设备名称在好友碰一碰时交换并展示给其他用户。
 * 通过手机 GATT 在首次连接时设置。
 *
 * @param name  设备名称（最多 7 字符，空字符结尾）。
 */
void nvs_config_set_device_name(const char *name);

/**
 * @brief 获取本设备名称。
 *
 * @return  设备名称字符串（从未设置则为空字符串）。
 */
const char *nvs_config_get_device_name(void);

/**
 * @brief 获取待同步到手机的联系人数量。
 *
 * @return  sync_state == CONTACT_SYNC_PENDING 的联系人数量。
 */
uint8_t nvs_config_get_pending_sync_count(void);

/**
 * @brief 将所有待同步联系人标记为已同步（手机确认后）。
 */
void nvs_config_mark_contacts_synced(void);

/**
 * @brief Get a contact by index (for iteration across all contacts).
 *
 * @param index  0-based index in the contacts array.
 * @return       Pointer to contact_alias_t, or NULL if index out of range.
 */
const contact_alias_t *nvs_config_get_contact_by_index(uint8_t index);

/**
 * @brief 翻转联系人的置顶状态（V6.32: 长按好友弹窗）。
 *
 * @param node_id  4 字节 Mesh 节点 ID。
 * @return         成功返回 true，未找到返回 false。
 */
bool nvs_config_toggle_pin_contact(uint32_t node_id);

/**
 * @brief 查询联系人是否置顶（V6.32）。
 *
 * @param node_id  4 字节 Mesh 节点 ID。
 * @return         已置顶返回 true。
 */
bool nvs_config_is_contact_pinned(uint32_t node_id);

/* ══════════════════════════════════════════════════════════════
 *  恢复出厂设置 (V6.19)
 * ══════════════════════════════════════════════════════════════ */

/**
 * @brief 恢复出厂设置：擦除全部 NVS 数据并重启。
 *
 * 清除所有用户数据：设备名、联系人、快捷消息、设置、
 * 摩斯教学进度、加密密钥、WouoUI 参数等。
 * 调用后设备自动重启，首次开机引导重新运行。
 *
 * @note 此函数不会返回（调用 esp_restart()）。
 */
void nvs_config_factory_reset(void);

/* ══════════════════════════════════════════════════════════════
 *  莫尔斯教学进度持久化（阶段八）
 * ══════════════════════════════════════════════════════════════ */

/** 每课最多星级 */
#define MORSE_MAX_STARS          3

/** 总课程数 */
#define MORSE_TOTAL_LESSONS      40

/** 莫尔斯教学进度 */
typedef struct {
    uint8_t  current_tier;          /**< 当前学习阶段 (0-4) */
    uint8_t  current_lesson;        /**< 阶段内课程索引 */
    uint16_t total_xp;              /**< 累计经验值 */
    uint8_t  streak;                /**< 当前连击数 */
    uint8_t  best_streak;           /**< 历史最高连击 */
    uint8_t  lesson_stars[MORSE_TOTAL_LESSONS];  /**< 每课星级 (0-3) */
    uint16_t lesson_attempts[MORSE_TOTAL_LESSONS]; /**< 每课尝试次数 */
    uint32_t last_study_ts;         /**< 上次学习 Unix 时间戳 */
} morse_progress_t;

/**
 * @brief 加载莫尔斯教学进度。
 *
 * 如果 NVS 中无数据（首次使用），初始化为零。
 * @param progress  [出] 填充的进度结构体。
 */
void nvs_config_load_morse_progress(morse_progress_t *progress);

/**
 * @brief 保存莫尔斯教学进度到 NVS。
 *
 * @param progress  要保存的进度结构体。
 */
void nvs_config_save_morse_progress(const morse_progress_t *progress);

#endif /* NVS_CONFIG_H */

/* ���� V9.0: MAC-based friend lookup ������������������������������������������������������ */
bool nvs_config_is_friend(const uint8_t *ble_mac);
