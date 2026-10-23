/**
 * @file assets.h
 * @brief 统一素材管理 — 所有位图/图标数组的 extern 声明
 *
 * 素材集中存放于 assets.c，此处提供外部引用声明。
 * 需要替换任何图标或图片时，只需修改 assets.c 中的数组数据。
 *
 * XBM 格式说明：
 *   - 每字节 8 个水平像素（LSB = 最左侧像素）
 *   - 数组维度：[行字节数 × 高度]
 *   - 行字节数 = ceil(宽度 / 8)
 *   - 使用 u8g2_DrawXBMP() 绘制
 *
 * 素材列表：
 *   main_icon_pic[][16*7]  — 32×28 主菜单磁贴图标（8 个）
 *   splash_img[16*32]      — 128×32 开机全屏图片
 */

#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 主菜单磁贴图标 (32×28, 112 字节/个, 共 8 个) ──────────── */
extern const uint8_t main_icon_pic[][16 * 7];

/* ── 开机全屏图片 (128×32, 512 字节, LSB 低位在前) ──────────── */
/* 使用 u8g2_DrawXBM() 绘制（非 DrawXBMP）                         */
extern const uint8_t splash_img[16 * 32];

#ifdef __cplusplus
}
#endif

#endif /* ASSETS_H */
