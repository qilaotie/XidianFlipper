#ifndef MENU_LAYOUT_H
#define MENU_LAYOUT_H
/* Single source of labels and destinations. Persistent page IDs stay unchanged. */
#define FUN_MENU_ITEMS(X) \
 X("- Pet and companion",M_FUN_PET) \
 X("- Solo games",M_FUN_SOLO) \
 X("- Nearby games",M_FUN_NEARBY) \
 X("- Multiplayer tabletop",M_PARTY_HUB) \
 X("- Light and sound ensemble",M_ENSEMBLE) \
 X("- How to play",M_GAME_HELP)
#define FUN_PET_ITEMS(X) \
 X("- Pet",M_PET) \
 X("- Pet gifts",M_PET_GIFTS) \
 X("- Pet travel passport",M_PASSPORT)
#define FUN_SOLO_ITEMS(X) \
 X("- Tetris portrait",M_TETRIS) \
 X("- Snake",M_SNAKE) \
 X("- Answer book",M_ANSWER_BOOK) \
 X("- Dice",M_DICE) \
 X("- Craps",M_CRAPS)
#define FUN_NEARBY_ITEMS(X) \
 X("- RPS nearby",M_RPS) \
 X("- Treasure radar",M_TREASURE) \
 X("- Reaction party",M_REACTION) \
 X("- Number bomb",M_NUMBER_BOMB) \
 X("- Time guess 1-10s",M_TIME_GUESS)
#define TOOLS_MENU_ITEMS(X) \
 X("- Remote controls",M_TOOLS_REMOTE) \
 X("- Teams and social",M_TOOLS_SOCIAL) \
 X("- Group activities",M_TOOLS_ACTIVITIES) \
 X("- Connections and cloud",M_TOOLS_CONNECT) \
 X("- Scan and diagnostics",M_TOOLS_DIAGNOSTIC)
#define TOOLS_REMOTE_ITEMS(X) \
 X("- Slide remote",M_HID_SLIDES) \
 X("- Media remote",M_HID_MEDIA) \
 X("- Camera remote",M_HID_CAMERA)
#define TOOLS_CONNECT_ITEMS(X) \
 X("- Cloud sync",M_CLOUD_SYNC) \
 X("- Network and connections",M_SETTINGS_NETWORK)
#define TOOLS_DIAGNOSTIC_ITEMS(X) \
 X("- Wi-Fi scanner",M_WIFI_SCAN) \
 X("- BLE scanner / RSSI",M_BLE_SCAN) \
 X("- Network diagnostics",M_NETWORK_DIAGNOSTICS) \
 X("- Network experiment",M_NETWORK_LAB) \
 X("- Network topology demo",M_NETWORK_TOPOLOGY) \
 X("- Memory status",M_TOOL_MEMORY)
#define TOOLS_SOCIAL_ITEMS(X) \
 X("- Teams and social",M_SOCIAL_HUB) \
 X("- Contact status",M_CONTACT_STATUS) \
 X("- Activity history",M_ACTIVITY_HISTORY)
#define TOOLS_ACTIVITIES_ITEMS(X) \
 X("- Multiplayer check-in",M_CHECKIN) \
 X("- Multiplayer poll",M_POLL) \
 X("- Team task board",M_TASK_BOARD) \
 X("- Quiz buzzer",M_QUIZ_BUZZER)
#define SETTINGS_NETWORK_ITEMS(X) \
 X("- Wi-Fi connect",M_WIFI_CONNECT) \
 X("- Local mesh network",M_NETWORK_STATUS) \
 X("- Channel select",M_CHANNEL_SELECT)
#define MENU_LABEL(label,page) {label},
#define MENU_PAGE(label,page) page,
#endif
