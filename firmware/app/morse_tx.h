/**
 * @file    morse_tx.h
 * @brief   摩斯电码发送模块 — V9.1 统一 mesh_packet 格式
 *
 * V9.1: 改用 mesh_packet_pack(MESH_TYPE_MORSE) 统一格式。
 *       单次扩展广播，不重试，不 ACK。
 *       实际发送由 morse_ui_send_plain_text() 处理，此模块保留备用。
 */

#ifndef MORSE_TX_H
#define MORSE_TX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 常量 ────────────────────────────────────────────────── */

/** 单包最大文本长度 */
#define MORSE_TX_MAX_TEXT_LEN    128

/* ── API ─────────────────────────────────────────────────── */

/**
 * @brief 初始化摩斯发送模块。
 */
void morse_tx_init(void);

/**
 * @brief 通过 BLE 广播发送明文字符串。
 *
 * 构造 payload [type|src_id|text_len|text]，
 * 通过 ble_radio_advertise_start() 单次广播。
 *
 * @param text   ASCII 文本。
 * @param len    文本长度（字节，最大 MORSE_TX_MAX_TEXT_LEN）。
 * @return 广播启动成功返回 true。
 */
bool morse_tx_send_plain(const char *text, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* MORSE_TX_H */
