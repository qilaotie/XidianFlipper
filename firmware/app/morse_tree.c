/* Tree nodes are generated from the shared codec, including valid punctuation. */
#include "morse_tree.h"
#include "morse_codec.h"
#include <string.h>
static MorseNode s_nodes[128];
static unsigned s_count;
static unsigned s_leaves;
static int s_layout_x[128], s_layout_depth[128];
static int layout(MorseNode *node, unsigned depth) {
    unsigned index = (unsigned)(node - s_nodes);
    s_layout_depth[index] = (int)depth;
    int left = -1, right = -1;
    if (node->dot_child)
        left = layout(node->dot_child, depth + 1);
    if (node->dash_child)
        right = layout(node->dash_child, depth + 1);
    int x = left < 0 && right < 0 ? (int)s_leaves++ * 1024
            : left < 0            ? right
            : right < 0           ? left
                                  : (left + right) / 2;
    s_layout_x[index] = x;
    return x;
}
void morse_tree_init(void) {
    memset(s_nodes, 0, sizeof(s_nodes));
    s_count = 1;
    const char *characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,?/=+-@";
    for (const char *c = characters; *c; ++c) {
        MorseNode *node = &s_nodes[0];
        const char *pattern = morse_codec_encode(*c);
        for (const char *p = pattern; p && *p; ++p) {
            MorseNode **child = *p == '.' ? &node->dot_child : &node->dash_child;
            if (!*child && s_count < sizeof(s_nodes) / sizeof(s_nodes[0])) {
                *child = &s_nodes[s_count++];
                (*child)->parent = node;
            }
            if (!*child)
                break;
            node = *child;
        }
        node->character = *c;
    }
    s_leaves = 0;
    layout(&s_nodes[0], 0);
}
MorseNode *morse_tree_get_root(void) {
    if (!s_count)
        morse_tree_init();
    return &s_nodes[0];
}
unsigned morse_tree_count(void) {
    morse_tree_get_root();
    return s_count;
}
MorseNode *morse_tree_node(unsigned index) {
    return index < morse_tree_count() ? &s_nodes[index] : NULL;
}
bool morse_tree_position(const MorseNode *node, int *x, int *y) {
    if (!node || !x || !y)
        return false;
    unsigned index = (unsigned)(node - s_nodes);
    if (index >= s_count)
        return false;
    *x = 2 + (s_leaves > 1 ? s_layout_x[index] * 59 / ((int)(s_leaves - 1) * 1024) : 29);
    *y = 1 + s_layout_depth[index] * 3;
    return true;
}
char morse_tree_walk(MorseNode **current, bool is_dash) {
    if (!current || !*current)
        return 0;
    MorseNode *next = is_dash ? (*current)->dash_child : (*current)->dot_child;
    if (!next)
        return 0;
    *current = next;
    return next->character;
}
void morse_tree_reset(MorseNode **current) {
    if (current)
        *current = &s_nodes[0];
}
char morse_tree_get_char(MorseNode *node) { return node ? node->character : 0; }
int morse_tree_get_pattern(MorseNode *node, char *buf, int max) {
    if (!node || !buf || max < 1)
        return 0;
    char reverse[16];
    int count = 0;
    while (node->parent && count < 16) {
        reverse[count++] = node->parent->dot_child == node ? '.' : '-';
        node = node->parent;
    }
    int len = count < max - 1 ? count : max - 1;
    for (int i = 0; i < len; ++i)
        buf[i] = reverse[count - i - 1];
    buf[len] = 0;
    return len;
}
