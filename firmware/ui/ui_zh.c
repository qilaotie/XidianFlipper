#include "ui_zh.h"
#include "ui_language.h"
#include <string.h>
extern const uint8_t halfstep_zh_font[];
typedef struct {const char *key,*value;} translation_t;
static const translation_t translations[]={
    {"+ Chinese UI","+ 中文界面"},{"+ Language SAVE ERROR","+ 语言保存失败"},
    {"- Skin: PIXEL","- 皮肤：经典像素"},{"- Skin: MINIMAL","- 皮肤：极简线框"},{"- Skin: TERMINAL","- 皮肤：终端风格"},
    {"- Home: MESSAGES","- 首页：消息"},{"- Home: PET","- 首页：宠物"},{"- Home: SCORES","- 首页：游戏纪录"},{"- Home: AUTO","- 首页：自动轮播"},
    {"- Style SAVE ERROR","- 外观保存失败"},{"Pet status","宠物状态"},
    {"- How to play","- 玩法说明"},
    {"- Pet","- 电子宠物"},{"- Tetris portrait","- 竖屏俄罗斯方块"},{"- Snake","- 贪吃蛇"},
    {"- Answer book","- 答案之书"},{"- Dice","- 普通骰子"},{"- Craps","- 花旗骰"},
    {"- RPS nearby","- 石头剪刀布"},{"- Treasure radar","- 寻宝雷达"},{"- Reaction party","- 多人反应赛"},
    {"- Number bomb","- 数字炸弹"},{"- Time guess 1-10s","- 盲猜秒数"},
    {"MESSAGES","消息"},{"MORSE LIVE","摩斯电台"},{"LEARN","学习"},{"FRIENDS","好友"},
    {"FUN","趣玩"},{"SETTINGS","设置"},{"ABOUT","关于"},{"TOOLS","工具"},
    {"Quick phrases","附近消息"},{"Key & listen","收发摩斯"},{"Practice + quiz","练习与测验"},
    {"Touch to add","靠近加好友"},{"Pet, arcade & party games","宠物与单人多人游戏"},
    {"Make it yours","调整设备设置"},{"Remotes, Wi-Fi & scanners","遥控联网与扫描"},
    {"Pet & party games","宠物与游戏"},{"Remote / Wi-Fi","遥控与联网"},
    {"~ Disp Bri","~ 屏幕亮度"},{"~ Btn SPT","~ 短按时长"},{"~ Btn LPT","~ 长按时长"},
    {"+ LED Enable","+ 灯光开关"},{"+ Buzzer Enable","+ 声音开关"},{"~ Morse WPM","~ 摩斯速度"},
    {"+ Text Scroll","+ 文字滚动"},{"- Factory Reset","- 恢复出厂"},{"- Screen / power","- 屏幕与电源"},
    {"Disp Bri","屏幕亮度"},{"Btn SPT","短按时长"},{"Btn LPT","长按时长"},{"Speed (WPM)","摩斯速度"},
    {"Factory Reset?","恢复出厂？"},{"Cancel","取消"},{"Confirm Reset","确认重置"},
    {"- Slide remote","- 幻灯片遥控"},{"- Media remote","- 媒体遥控"},{"- Camera remote","- 拍照遥控"},
    {"- Wi-Fi connect","- 无线网络连接"},{"- Cloud sync","- 云端同步"},
    {"- Wi-Fi scanner","- 无线网络扫描"},{"- BLE scanner / RSSI","- 蓝牙扫描与信号"},{"- Memory status","- 运行内存"},
    {"- Scan / choose Wi-Fi","- 扫描选择网络"},{"- Connection status","- 连接状态"},
    {"- Connect saved","- 连接已存网络"},{"- Disconnect","- 断开连接"},{"- Forget saved","- 忘记网络"},
    {"- Auto reconnect: ON","- 自动重连：开"},{"- Auto reconnect: OFF","- 自动重连：关"},
    {"- Keep saved network","- 保留网络"},{"- Forget + disconnect","- 忘记并断开"},
    {"- Screen off now","- 立即关屏"},{"- Deep sleep: hold key","- 深度睡眠"},
    {"MEDIA REMOTE","媒体遥控"},{"SLIDE REMOTE","幻灯片遥控"},
    {"PAIR: Halfstep Remote","配对 Halfstep Remote"},{"MESSAGING PAUSED","附近通信已暂停"},
    {"FINISHING WI-FI SCAN","等待网络扫描结束"},{"RADIO STARTING...","蓝牙正在启动"},
    {"WAITING FOR MESSAGES","等待消息发完"},{"BACK: EXIT","返回键退出"},
    {"REMOTE UNAVAILABLE","遥控暂不可用"},{"BACK: RETRY","返回重试"},
    {"BACK: EXIT / RETRY","返回退出或重试"},{"WAITING: MEDIA REPORT","等待媒体键订阅"},
    {"WAITING: KEYBOARD REPORT","等待键盘订阅"},{"WAIT FOR HOST...","等待主机连接"},
    {"TURN:TRY ANOTHER KEY","转动切换按键"},{"TURN:KEY CLICK:SHOOT","转选按键 按下拍照"},
    {"HOLD:PULSE BACK:EXIT","长按改时长 返回退出"},{"TURN:VOL SP:PLAY/PAUSE","转调音量 按下播放"},
    {"TURN:PREV/NEXT SP:SPACE","转动翻页 按下空格"},{"HOLD:NEXT TRACK","长按下一曲"},
    {"HOLD:START SLIDES (F5)","长按放映 F5"},{"SCANNING 2.4GHz...","正在扫描网络"},
    {"BACK: CANCEL VIEW","返回退出查看"},{"NO HOTSPOTS FOUND","未找到热点"},
    {"HOLD:SCAN BACK:EXIT","长按扫描 返回退出"},{"HOLD:RETRY BACK:EXIT","长按重试 返回退出"},
    {"SP:SIGNAL HOLD:CLEAR","按看信号 长按清空"},{"Scanning broadcasts...","正在扫描广播"},
    {"DEVICE EXPIRED","设备信号已过期"},{"BACK: DEVICE LIST","返回设备列表"},
    {"HOLD: SHARE ON/OFF","长按开关公开分享"},{"CLICK: SYNC NOW","按下立即同步"},
    {"HOLD: REMOVE ONLINE","长按撤下公开数据"},{"CLICK: WI-FI SETTINGS","按下进入网络设置"},
    {"WI-FI BUSY / RETRY","网络忙 请重试"},{"NO SAVED NETWORK","没有已存网络"},
    {"CONNECT AND SAVE FIRST","请先连接并保存"},{"SAVE/REMOVE ERROR","保存或撤下失败"},
    {"ENABLE SHARING FIRST","请先开启分享"},{"CLOUD SAVE ERROR","云端配置保存失败"},
    {"PUBLIC DATA: HOLD TO SET","公开数据 长按设置"},{"HOLD:REMOVE ONLINE DATA","长按撤下公开数据"},
    {"KEY SENT - CHECK PHONE","已发按键 请看手机"},{"KEY BUSY / NOT SENT","按键忙 尚未发送"},
    {"RUNTIME HEAP","运行内存"},{"CONNECTING / DHCP...","正在连接并获取地址"},
    {"WI-FI DISCONNECTED","网络已断开"},{"BACK:MENU HOLD:STOP","返回菜单 长按断开"},
    {"SHARING OFF","分享已关闭"},{"REMOVE PENDING","等待撤下"},{"WAIT FOR WI-FI","等待网络连接"},
    {"SYNC OK / AUTO 15M","同步成功 每15分自动"},{"REMOVED / SHARE OFF","已撤下并关闭分享"},
    {"WAIT FOR NET TIME","等待网络校时"},{"CREDENTIAL REJECTED","设备凭证被拒绝"},
    {"RATE LIMIT / RETRY","请求频繁 稍后重试"},{"NETWORK ERROR / RETRY","网络失败 稍后重试"},
    {"REMOVE: NEED WI-FI","联网后撤下"},{"LOCAL DATA ERROR","本机数据错误"},
    {"REMOVING ONLINE...","正在撤下公开数据"},{"SYNCING HTTPS...","正在安全同步"},
    {"NO MEMORY / RETRY","内存不足 稍后重试"},{"NOT SHARED YET","尚未分享"},
};
const char *ui_zh_lookup(const char *text){
    if(!text)return "";
    if(!ui_language_chinese())return text;
    for(unsigned i=0;i<sizeof(translations)/sizeof(translations[0]);i++)
        if(!strcmp(text,translations[i].key))return translations[i].value;
    return text;
}
static bool chinese_text(const char *text){
    for(;*text;text++)if((unsigned char)*text>=128)return true;
    return false;
}
u8g2_uint_t ui_zh_width(u8g2_t *display,const char *text){
    const char *translated=ui_zh_lookup(text);
    if(!chinese_text(translated))return u8g2_GetStrWidth(display,translated);
    const uint8_t *previous=display->font;u8g2_SetFont(display,halfstep_zh_font);
    u8g2_uint_t width=u8g2_GetUTF8Width(display,translated);u8g2_SetFont(display,previous);return width;
}
void ui_zh_draw(u8g2_t *display,u8g2_int_t x,u8g2_int_t baseline,const char *text){
    const char *translated=ui_zh_lookup(text);
    if(!chinese_text(translated)){u8g2_DrawStr(display,x,baseline,translated);return;}
    const uint8_t *previous=display->font;u8g2_SetFont(display,halfstep_zh_font);
    u8g2_DrawUTF8(display,x,baseline,translated);u8g2_SetFont(display,previous);
}
