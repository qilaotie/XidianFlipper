#ifndef PET_HELP_H
#define PET_HELP_H
#include "pet_games.h"
enum {PET_HELP_QUICK=PG_COUNT,PET_HELP_VISIT,PET_HELP_DUEL};
unsigned pet_help_pages(unsigned item);
const char *pet_help_line(unsigned item,unsigned page,unsigned row);
#endif
