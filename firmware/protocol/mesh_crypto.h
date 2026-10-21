/**
 * @file    mesh_crypto.h
 * @brief   BLE Mesh 载荷 AES-128-CCM 加密/解密 — V5.4
 *
 * 历史设计见 docs/archive/pre-v12/项目架构.md §8.4。
 * 当前消息路径是否使用加密以 docs/local-architecture.md 为准，不能从本头文件推断。
 *
 * 密钥管理：
 *   - 阶段二：硬编码 16 字节开发密钥（MESH_DEV_KEY）
 *   - V9.0.1: 手机 GATT 已移除，密钥通过 NVS 默认值管理
 *
 * Nonce 构造（13 字节，依据 §14.二.2）：
 *   SRC_ID(4B) | DST_ID(4B) | MSG_ID_LO(2B) | SEQ(2B) | PAD(1B)
 *
 * 防重放（§14.二.2）：
 *   单调递增 SEQ 计数器（RTC_DATA_ATTR）。接收方对照邻居缓存
 *   检查 SRC_ID + SEQ；若 SEQ ≤ 上次见到的值则丢弃。
 *
 * CCM 参数：
 *   - AES 密钥：128 位（16 字节）
 *   - Nonce：13 字节
 *   - MIC 标签：4 字节（32 位）
 *
 * 硬件加速：ESP32-C6 AES 加速器（通过 mbedTLS CCM）。
 */

#ifndef MESH_CRYPTO_H
#define MESH_CRYPTO_H

#include "mesh_packet.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 常量 ────────────────────────────────────────────────── */

/** AES 密钥长度（字节） */
#define MESH_AES_KEY_LEN        16

/** AES-128-CCM Nonce 长度（字节）— 依据 §14.二.2 */
#define MESH_CCM_NONCE_LEN      13

/** MIC / 认证标签长度（字节） */
#define MESH_CCM_MIC_LEN        MESH_MIC_SIZE

/* ── 开发密钥（仅阶段二；阶段四 → 手机配置密钥）───────────── */
/* 16 字节 AES-128 密钥："BLUELONG_MESH_P2"，末尾补零 */
#define MESH_DEV_KEY { \
    0x42, 0x4C, 0x55, 0x45,   /* "BLUE" */ \
    0x4C, 0x4F, 0x4E, 0x47,   /* "LONG" */ \
    0x5F, 0x4D, 0x45, 0x53,   /* "_MES" */ \
    0x48, 0x5F, 0x50, 0x32    /* "H_P2" */ \
}

/* ── 防重放序列号（RTC_DATA_ATTR）────────────────────────── */

/** SEQ 回绕前的最大值（2^16 - 1），溢出后回绕到 0。 */
#define MESH_SEQ_MAX            0xFFFF

/* ── API ─────────────────────────────────────────────────── */

/**
 * @brief 初始化加密子系统。
 *
 * 将 AES 密钥加载到 mbedTLS CCM 上下文中。
 * 必须在启动后、任何加解密操作前调用一次。
 */
void mesh_crypto_init(void);

/**
 * @brief 获取下一个单调序列号并递增。
 *
 * SEQ 存储在 RTC_DATA_ATTR 中，跨 DeepSleep 保持。
 * 从 MESH_SEQ_MAX 回绕到 0。
 *
 * @return 当前 SEQ 值（递增前）。
 */
uint16_t mesh_crypto_seq_next(void);

/**
 * @brief 用 AES-128-CCM 加密 Mesh 数据包载荷。
 *
 * 原地修改 pkt：
 *   - pkt->payload[]：明文 → 密文（长度不变）
 *   - pkt->flags：置位 MESH_FLAG_ENCRYPTED
 *   - pkt->mic[]：填充 4 字节 CCM 认证标签
 *
 * 调用者需确保 pkt 已完整填充（src_id、dst_id、
 * msg_id、type、flags、payload、len）。CRC16 不需要 —
 * MIC 取代其功能。
 *
 * @param[in,out] pkt   待加密的数据包（原地修改）。
 * @param         seq   用于 Nonce 的序列号（来自 mesh_crypto_seq_next()）。
 *
 * @return 成功返回 true，错误返回 false。
 */
bool mesh_crypto_encrypt_packet(mesh_packet_t *pkt, uint16_t seq);

/**
 * @brief 解密 Mesh 数据包载荷并验证 MIC。
 *
 * 原地修改 pkt：
 *   - pkt->payload[]：密文 → 明文（长度不变）
 *   - pkt->flags：清除 MESH_FLAG_ENCRYPTED
 *   - pkt->crc16：置 0（调用者如需 CRC 应自行计算）
 *
 * @param[in,out] pkt   加密的数据包（原地修改）。
 * @param         seq   加密时使用的序列号（来自缓存）。
 *
 * @return MIC 验证成功且解密完成返回 true，
 *         MIC 不匹配（篡改/密钥错误）返回 false。
 */
bool mesh_crypto_decrypt_packet(mesh_packet_t *pkt);

/**
 * @brief 检查 AES 密钥是否为默认开发密钥。
 *
 * @return 当前密钥匹配 MESH_DEV_KEY 返回 true。
 */
bool mesh_crypto_is_dev_key(void);

/**
 * @brief 将当前 AES 密钥替换为手机配置的密钥。
 *
 * V9.0.1: GATT 已移除，此函数保留为桩实现。
 * 新密钥存储到 NVS 以持久化，活跃的 CCM 上下文立即更换密钥。
 *
 * @param key  16 字节 AES-128 密钥（MESH_AES_KEY_LEN）。
 */
void mesh_crypto_set_key(const uint8_t key[MESH_AES_KEY_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* MESH_CRYPTO_H */
