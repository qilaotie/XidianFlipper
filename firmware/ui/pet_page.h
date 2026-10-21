#ifndef PET_PAGE_H
#define PET_PAGE_H
#include <stdint.h>
#include <stdbool.h>
void pet_page_enter(uint8_t page);
void pet_page_proc(void);
bool pet_page_allows_attention(void);
#endif
