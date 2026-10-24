#ifndef DRAFT_STORE_H
#define DRAFT_STORE_H
#include <stdbool.h>
#define DRAFT_SLOTS 6
#define DRAFT_TEXT_BYTES 129
const char *draft_store_work(void);
const char *draft_store_slot(unsigned slot);
bool draft_store_save_work(const char *text);
bool draft_store_save_slot(unsigned slot,const char *text);
#endif
