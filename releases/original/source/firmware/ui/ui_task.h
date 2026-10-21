/**
 * @file    ui_task.h
 * @brief   UI FreeRTOS 任务 — V5.4 阶段三
 *
 * 以约 30Hz 频率运行 WouoUI 主循环。
 * 从 g_input_queue 消费输入事件。
 * 空闲超时后触发 DeepSleep。
 */

#ifndef UI_TASK_H
#define UI_TASK_H

/**
 * @brief 创建并启动 UI 任务。
 *
 * 优先级：中等（低于 radio_task，高于 battery_task）。
 * 栈大小：4 KB。
 */
void ui_task_create(void);

#endif /* UI_TASK_H */
