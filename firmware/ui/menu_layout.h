#ifndef MENU_LAYOUT_H
#define MENU_LAYOUT_H
/* Single source of labels and destinations. Persistent page IDs stay unchanged. */
#define FUN_MENU_ITEMS(X) \
 X("- Pet",M_PET) \
 X("- Tetris portrait",M_TETRIS) \
 X("- Snake",M_SNAKE) \
 X("- Answer book",M_ANSWER_BOOK) \
 X("- Dice",M_DICE) \
 X("- Craps",M_CRAPS) \
 X("- RPS nearby",M_RPS) \
 X("- Treasure radar",M_TREASURE) \
 X("- Reaction party",M_REACTION) \
 X("- Number bomb",M_NUMBER_BOMB) \
 X("- Time guess 1-10s",M_TIME_GUESS)
#define TOOLS_MENU_ITEMS(X) \
 X("- Slide remote",M_HID_SLIDES) \
 X("- Media remote",M_HID_MEDIA) \
 X("- Camera remote",M_HID_CAMERA) \
 X("- Wi-Fi connect",M_WIFI_CONNECT) \
 X("- Wi-Fi scanner",M_WIFI_SCAN) \
 X("- BLE scanner / RSSI",M_BLE_SCAN) \
 X("- Memory status",M_TOOL_MEMORY)
#define MENU_LABEL(label,page) {label},
#define MENU_PAGE(label,page) page,
#endif
