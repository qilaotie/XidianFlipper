/**
 * @file    morse_tree.h
 * @brief   莫尔斯码二叉树 — ITU 标准莫尔斯码解码器
 *
 * 采用静态二叉树结构，每个节点包含：
 *   - character: 当前节点对应的字符（'\\0' 表示中间节点）
 *   - dot_child:  输入"点"后的子节点
 *   - dash_child: 输入"划"后的子节点
 *
 * 根节点 character='\\0'，沿点划下行，叶节点返回对应字符。
 * 未知序列返回 '?'。
 *
 * 覆盖：A-Z, 0-9, ., ,, ?, /, =, +, -, @
 */

#ifndef MORSE_TREE_H
#define MORSE_TREE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 莫斯树节点 ─────────────────────────────────────────── */

typedef struct MorseNode {
    char                character;      /**< 当前节点字符（'\\0' = 中间节点, '?' = 无效） */
    struct MorseNode   *dot_child;      /**< 点(·)子节点 */
    struct MorseNode   *dash_child;     /**< 划(—)子节点 */
    struct MorseNode   *parent;         /**< 父节点（用于回溯路径） */
} MorseNode;

/* ── API ────────────────────────────────────────────────── */

/**
 * @brief 初始化莫斯树（构建完整 ITU 二叉树）。
 *
 * 在 morse_terminal_init() 中调用一次即可。
 * 所有节点静态分配，无需释放。
 */
void morse_tree_init(void);

/**
 * @brief 获取根节点指针。
 * @return 根节点（character='\\0'）。
 */
MorseNode *morse_tree_get_root(void);

/**
 * @brief 沿树走一步。
 *
 * @param current  当前节点指针的指针（会被更新为新节点）。
 * @param is_dash  true=划(-), false=点(.)。
 * @return 新节点的字符。若为 '\\0' 表示中间节点（尚未形成有效字符）。
 *         若对应子节点为 NULL，返回 '?' 且 current 不变。
 */
char morse_tree_walk(MorseNode **current, bool is_dash);

/**
 * @brief 复位遍历指针到根节点。
 * @param current  当前节点指针的指针（会被设为根节点）。
 */
void morse_tree_reset(MorseNode **current);

/**
 * @brief 获取节点的字符。
 * @param node  节点指针。
 * @return 节点字符（'\\0' 表示无效/中间节点）。
 */
char morse_tree_get_char(MorseNode *node);

/**
 * @brief 回溯获取当前节点的完整点划序列。
 *
 * 从当前节点沿 parent 链回溯到根，反转后写入 buf。
 *
 * @param node  当前节点。
 * @param buf   输出缓冲区。
 * @param max   缓冲区最大长度（含 '\\0'）。
 * @return 序列长度（不含 '\\0'），0 表示根节点或错误。
 */
int morse_tree_get_pattern(MorseNode *node, char *buf, int max);
unsigned morse_tree_count(void);
MorseNode *morse_tree_node(unsigned index);
/* Complete-tree minimap coordinates: x=2..61, y=1..19; one fixed context for every depth. */
bool morse_tree_position(const MorseNode *node, int *x, int *y);

#ifdef __cplusplus
}
#endif

#endif /* MORSE_TREE_H */
