#ifndef HALFSTEP_HELP_TEXT_H
#define HALFSTEP_HELP_TEXT_H
unsigned ui_pet_help_pages(unsigned item);
const char *ui_pet_help_line(unsigned item,unsigned page,unsigned row);
#define GAME_GUIDES 11
#define PET_GUIDES 13
const char *pet_guide_name(unsigned item);
unsigned pet_guide_pages(unsigned item);
const char *pet_guide_line(unsigned item,unsigned page,unsigned row);
const char *game_guide_name(unsigned item);
unsigned game_guide_pages(unsigned item);
const char *game_guide_line(unsigned item,unsigned page,unsigned row);
#endif
