/**
 * @file    stress_test.h
 * @brief   混合模式压力测试框架 — V5.4 阶段五
 *
 * GATT + Mesh 并发操作的内部压力测试模块。
 * 由 main.c 中的 PHASE5_STRESS_TEST_ENABLED 宏控制。
 *
 * 不包含在生产固件中。仅用于调试和极端场景验证，
 * 依据第12节阶段五测试流程。
 */

#ifndef STRESS_TEST_H
#define STRESS_TEST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化压力测试模块。
 *
 * 设置测试计数器并准备模拟工作负载模式。
 * 当 PHASE5_STRESS_TEST_ENABLED 置位时，启动时调用一次。
 */
void stress_test_init(void);

/**
 * @brief 周期性压力测试节拍（从主循环调用）。
 *
 * 注入模拟工作负载：
 *   - 高频 GATT 写入（模拟）
 *   - 高频 Mesh 洪泛数据包（模拟）
 *   - 跟踪丢包率和连接稳定性
 *
 * 每 30 秒输出一次统计日志。
 */
void stress_test_tick(void);

/**
 * @brief 获取当前压力测试统计信息。
 *
 * @param pkt_loss_rate      [出] Mesh 丢包率（0.0 ~ 1.0）。
 * @param gatt_disconnects   [出] 累计 GATT 断开连接次数。
 * @param mesh_drops         [出] 累计 Mesh 丢包数。
 */
void stress_test_get_stats(float *pkt_loss_rate,
                           uint32_t *gatt_disconnects,
                           uint32_t *mesh_drops);

#ifdef __cplusplus
}
#endif

#endif /* STRESS_TEST_H */
