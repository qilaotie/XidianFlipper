#include "ui_zh.h"
#include "ui_language.h"
#include <string.h>
extern const uint8_t halfstep_zh_font[];
typedef struct {const char *key,*value;} translation_t;
static const translation_t translations[]={
    {"WAIT...","等待信号"},{"GO! TAP","开始 按下"},{"SP WITHIN 1.2s","1.2秒内短按"},{"SP: GAME MENU","短按回游戏"},{"REMEMBER","记住顺序"},{"LEFT TURN","向左转"},{"RIGHT TURN","向右转"},{"TAP","短按"},{"REPEAT THE ORDER","按顺序重复"},{"POWER %u","力量 %u"},{"SP: POWER>=7","力到七再按"},{"TIME %us TAP","%u秒 连续短按"},{"YOU %u BALL %u","你%u 球%u"},{"SP: LAST 300ms","最后300ms按"},{"TURN TO CATCH","转动换道接住"},
    {"- Practice","- 自由练习"},{"- Tier 0: First Steps","- 入门"},{"- Tier 1: Building","- 基础"},{"- Tier 2: Full Alphabet","- 完整字母"},{"- Tier 3: Numbers","- 数字"},{"- Tier 4: Pro Signals","- 专业信号"},{"- [ Free Practice ]","- 自由练习"},
    {"STEP BY STEP","循序渐进"},{"DO IT TODAY","今天就做"},{"TOMORROW","明天再说"},{"LUCK IS NEAR","好运将至"},{"POINT: %u","目标 %u"},{"PLAYER WAIT","等待"},
    {"The Book of Answers","答案之书"},{"Hold a question...","心中默想一个问题"},{"YOUR ANSWER","你的答案"},
    {"YES","可以"},{"NO","不行"},{"MAYBE","也许"},{"ASK AGAIN","再问一次"},{"GO FOR IT","大胆去做"},{"JUST DO IT","现在行动"},{"WAIT A BIT","等一等"},{"NOT NOW","暂时不要"},{"BE PATIENT","保持耐心"},{"LET IT GO","放下执念"},{"DON'T RUSH","不要着急"},{"THINK TWICE","三思而行"},{"FOLLOW HEART","跟随内心"},{"ASK A FRIEND","问问朋友"},{"SLEEP ON IT","明天再想"},{"TAKE A BREAK","休息一下"},{"CARPE DIEM","把握当下"},{"WHY NOT","有何不可"},{"SURE THING","当然可以"},{"ABSOLUTELY","毫无疑问"},{"NEVER MIND","不必介意"},{"FORGET IT","算了吧"},{"TRY LATER","稍后再试"},{"GIVE IT TIME","交给时间"},{"STAY CALM","保持冷静"},{"KEEP GOING","继续前进"},{"HOLD ON","再坚持下"},{"TRUST FATE","相信缘分"},{"ROLL WITH IT","顺其自然"},{"BIG YES","非常赞成"},{"BIG NO","坚决不要"},{"100% YES","肯定可以"},{"RISKY MOVE","风险较大"},{"PLAY IT SAFE","稳妥为先"},{"CHANGE PLANS","换个计划"},{"FOCUS FIRST","先专注"},
    {"POINT %u","目标 %u"},{"RETRY AVAILABLE","短按重试"},{"MESSAGE STATUS","消息发送状态"},{"Retry rejected","暂时无法重试"},
    {"BOMB NUMBER: %u","炸弹数字 %u"},{"OUT: %s","出局 %s"},{"RANGE %u..%u","范围 %u..%u"},{"TURN: %s","轮到 %s"},{"< %u > PRESS:GUESS","< %u > 短按猜数"},
    {"RANK %u: %u ms","第%u名 %u毫秒"},{"TARGET: %u SECONDS","目标 %u秒"},{"YOU: %u ms","你用时 %u毫秒"},{"YOU: %u.%03u s","你用时 %u.%03u秒"},{"TARGET %us / ARMING","目标%u秒 准备开始"},{"ALL READY: START IN %lu","全员准备 %lu秒后开始"},{"ALL READY: START IN %u","全员准备 %u秒后开始"},
    {"%s %u/4 READY %u","%s %u/4 已备%u"},{"HOST","房主"},{"READY","准备"},{"FALSE START","抢跑"},{"DNF / DISCONNECTED","未完成或已失联"},{"REACTION PARTY","多人反应赛"},{"FALSE START!","抢跑了"},{"NO RESPONSE","没有响应"},{"WAIT FOR ALL RESULTS","等待所有人成绩"},{"GO! COUNT SILENTLY","开始 请默数"},{"WAIT FOR GO...","等待开始信号"},{"PRESS TO STOP","短按停止计时"},{"DON'T PRESS YET","现在不要按"},{"*** GO! PRESS NOW ***","开始 现在短按"},{"WAIT... DON'T PRESS","等待 不要按"},{"LOCKING PLAYER LIST...","正在锁定玩家名单"},
    {"Invalid code","无效编码"},
    {"SNAKE CLASSIC","经典地图"},{"TETRIS PLAY","开始"},{"PAUSED","暂停"},{"SAVEERR","存错"},{"OVER","结束"},{"CONT","继续"},{"NEW","新局"},{"EXIT","退出"},
    {"ROCK","石头"},{"SCISSORS","剪刀"},{"PAPER","布"},{"PICK","出拳"},{"PICK LOCKED","已锁定"},{"DRAW","平局"},{"WIN","胜利"},{"LOSE","失败"},
    {"NUMBER BOMB 2-4","数字炸弹 二到四人"},{"TIME GUESS 1-10s","盲猜一到十秒"},{"REACTION PARTY 2-4","反应赛 二到四人"},{"< JOIN ROOM >","< 加入房间 >"},{"< CREATE ROOM >","< 创建房间 >"},{"TURN:SELECT PRESS:OPEN","转动选择 短按进入"},
    {"ROOM CLOSED / LOST","房间关闭或失联"},{"PRESS: RETURN TO MENU","短按返回菜单"},{"SEARCHING FOR ROOM","正在寻找房间"},{"HOST: CREATE ROOM","请房主创建房间"},{"ROOM FULL","房间已满"},{"JOINING ROOM...","正在加入房间"},{"WAIT FOR HOST","等待房主"},{"WAIT FOR YOUR TURN","等待轮到自己"},{"SUBMITTING...","正在提交"},
    {"NEXT READY / WAITING","已准备 等待下一局"},{"PRESS:NEXT BACK:EXIT","短按下一局 返回退出"},{"TURN:SCORES PRESS:NEXT","转动看分 短按下一局"},{"READY / WAIT FOR HOST","已准备 等待房主"},{"PRESS & RELEASE:READY","短按并松开准备"},
    {"BOTH OPEN; SP: INVITE","双方进入 短按邀请"},{"WAIT PEER ACCEPT...","等待对方同意"},{"SP: ACCEPT BACK: NO","短按同意 返回拒绝"},{"WAIT START...","等待开始"},{"LEFT/NOT READY SP:RETRY","离线或未就绪 按重试"},{"SP: AGAIN BACK: EXIT","短按再来 返回退出"},{"WAIT PEER RESULT...","等待对方成绩"},{"TAP FAST! BACK: EXIT","快速短按 返回退出"},{"VISIT: NO GIFT / LIMIT","拜访成功 奖励冷却中"},
    {"TRAINING SPEED","练习速度"},{"COMPLETED","已完成"},{"READY TO LEARN","准备学习"},{"CORRECT!","答对了"},{"LESSON COMPLETE","课程完成"},{"Next   [EXIT]","继续  [退出]"},{"[NEXT]   Exit","[继续]  退出"},{"SOUND IS MUTED","声音已关闭"},{"LISTEN...","请听声音"},{"Choose what you heard","选择听到的字母"},{"FROM MEMORY","凭记忆输入"},{"YOUR TURN","轮到你了"},{"Timing: keep steady","请保持节奏稳定"},
    {"- Inbox (%u new)","- 收件箱（%u条未读）"},{"- Nearby room %02u","- 附近频道 %02u"},
    {"GET NOTES","接音符"},{"FOOD %u MOOD %u","饱%u 心%u"},{"REST %u WASH %u","精%u 洁%u"},{"CARE %u/24","照顾 %u/24"},{"WINS %u/5","训练 %u/5"},
    {"%uG SP:BUY+EQUIP","%uG 按下购买"},{"%uG SP:BUY","%uG 按下购买"},{"x%u SP:USE","数量%u 按下用"},{"WAIT / SAVE ERR","稍等或检查保存"},
    {"- Care and controls","- 照顾与操作"},{"- Growth and naming","- 成长与起名"},{"- Shop and rewards","- 商店与奖励"},{"- Rooms and idle actions","- 房间与待机动作"},
    {"- Catch notes","- 接音符"},{"- Bump","- 相扑"},{"- Flags","- 举旗"},{"- Heading","- 顶皮球"},{"- Memory","- 按键记忆"},{"- Sprint","- 赛跑"},{"- Quick tap","- 快速反应"},{"- Visit","- 联机拜访"},{"- Duel","- 联机对战"},
    {"- Free practice","- 自由练习"},{"- Listen & choose","- 听音选字"},{"- Follow a letter","- 跟练字母"},{"- Write a letter","- 听写字母"},
    {"- Quick phrases","- 快捷短语"},{"- Last send / retry","- 发送状态与重试"},{"- Nearby room","- 附近频道"},{"- Write message","- 编辑消息"},{"- ASCII faces","- 字符表情"},
    {"- Live Morse","- 实时摩斯"},{"- Received messages","- 已收消息"},{"- Touch to add","- 靠近添加好友"},{"- Keep friend","- 保留好友"},{"- Revoke friendship","- 确认解除好友"},{"- Send quick phrase","- 发送快捷短语"},{"- Remove friend","- 移除好友"},
    {"Nearby talk room","附近通信频道"},{"Saving room...","正在保存频道"},{"Same room = talk together","相同频道即可通信"},{"Channel save failed","频道保存失败"},
    {"No previous send","还没有发送记录"},{"Draft is empty","草稿为空"},{"Busy / send rejected","设备忙 发送被拒绝"},{"Maximum 128 characters","最多一百二十八字符"},
    {"Inbox is empty","收件箱为空"},{"Waiting for a message","等待接收消息"},{"Message status","消息发送状态"},{"Unknown","未知"},{"Queued","已排队"},{"Broadcasting","正在广播"},{"Sent","已发送"},{"Awaiting receipt","等待回执"},{"Peer received","对方已收到"},{"Failed","发送失败"},
    {"Pairing unavailable","暂时无法添加好友"},{"Friend unavailable","好友暂不可用"},{"Touch to add","靠近添加好友"},{"Bring devices close","请将设备靠近"},{"Add this device?","添加这台设备？"},{"Waiting for peer","等待对方确认"},{"Saving both sides","双方正在保存"},{"Pending sync","等待同步"},{"Friends confirmed","好友添加成功"},{"Pairing timed out","添加好友超时"},{"Pairing failed","添加好友失败"},{"Both open this page","双方都打开添加页面"},{"Confirm friendship?","短按确认添加好友"},{"BOTH SIDES MUST AGREE","需要双方同意"},{"Confirm failed","确认失败"},{"Revoke failed","解除好友失败"},
    {"Ready - release key","已唤醒 请松开按键"},{"REPLAY AVAILABLE","短按重播摩斯"},{"MORSE AUDIO","正在播放摩斯"},{"Text has no Morse code","文本没有可播放摩斯"},
    {"- Last modification time of firmware","- 固件信息"},{"- UI: THANK WouoUI","- 界面感谢 WouoUI"},{"- The software and hardware are all written by the students here","- 学生开发软硬件"},{"- Based on ESP32-C6, using BLE Mesh and NimBLE","- 基于 ESP32-C6 与 NimBLE"},{"- BLE Mesh v7.4","- 自定义蓝牙广播通信"},
    {"FEED","喂食"},{"PLAY","玩耍"},{"CLEAN","清洁"},{"SLEEP","睡觉"},{"WAKE","唤醒"},{"SHOP","商店"},{"BAG","背包"},{"DEX","图鉴"},{"GROWTH","成长"},{"CALLS","提醒"},{"CONNECT","联机"},{"HOW TO","说明"},{"NAME","改名"},{"ROOM","房间"},{"CALLS OFF","提醒关"},{"CALLS ON","提醒开"},
    {"CLASSIC","原房间"},{"STUDY","书房"},{"GARDEN","花园"},{"SPACE","太空"},
    {"APPLE","苹果"},{"SNACK","零食"},{"SOAP","肥皂"},{"BALL","玩具球"},{"PLANT","盆栽"},{"STARS","星饰"},{"RUG","地毯"},{"NO DECOR","移除装饰"},
    {"EGG","蛋"},{"BABY","幼体"},{"TEEN","少年"},{"SUNNY","阳光型"},{"SPORT","运动型"},{"MELLOW","温和型"},
    {"PET GAMES","宠物游戏"},{"HOW TO PLAY","玩法说明"},{"CONNECTION","联机互动"},{"QUICK TAP","快速反应"},{"VISIT","拜访"},{"DUEL","对战"},{"NOTES","接音符"},{"BUMP","相扑"},{"FLAGS","举旗"},{"HEADING","顶球"},{"MEMORY","记忆"},{"SPRINT","赛跑"},
    {"OWNED","已拥有"},{"NOT OWNED","未购买"},{"EQUIPPED","已装备"},{"OWNED SP:EQUIP","短按装备"},{"READ ONLY","短按查看"},{"BOTH SAME MODE","双方选同模式"},{"WIN: +8G","获胜得八币"},{"DISCOVERED","已发现"},{"LOCKED","未发现"},
    {"HAPPY!","真开心"},{"BOUGHT!","购买成功"},{"ROOM EQUIPPED","房间已装备"},{"SAVE ERROR","保存失败"},{"SAVING...","保存中"},{"WAIT","稍等再试"},{"WAIT / WAKE UP","稍等或先唤醒"},{"NO GOLD / OWNED","缺币或已拥有"},{"NO GOLD/SAVE ERR","缺币或保存失败"},{"NEED REST / WAIT","需要休息"},{"WIN +8G","胜利加八币"},{"TRY AGAIN","再试一次"},{"TOO EARLY","按得太早"},{"TOO SLOW","按得太慢"},
    {"SP: BUY","短按购买"},{"SP: USE","短按使用"},{"SP: EQUIP","短按装备"},{"SP: OPEN","短按打开"},{"SP: READ","短按查看"},{"SP: START","短按开始"},{"SP: KEEP","保留宠物"},{"SP: NEW EGG","确认换蛋"},{"RETIRE THIS PET?","重新养蛋？"},{"NEW EGG?","重新养蛋"},{"KEEP PET","保留宠物"},{"NEW EGG","确认换蛋"},{"KEEP GOLD / DEX","金币图鉴保留"},
    {"SLEEPING","正在睡觉"},{"HUNGRY","肚子饿了"},{"TIRED","需要休息"},{"NEED WASH","需要清洁"},{"FEELING GOOD","状态良好"},
    {"TREASURE: HIDING","寻宝：正在隐藏"},{"ONE HIDES, MANY HUNT","一人藏 多人找"},{"TURN: HUNT  BACK: EXIT","转动寻找 返回退出"},{"TARGET LOST","目标已失联"},{"RADAR: SEARCHING","正在寻找宝藏"},{"PEER: TURN TO HIDING","请让对方选择隐藏"},{"TURN: HIDE SP:LOCK","转动隐藏 短按锁定"},{"SP:LOCK LP:MUTE","短按锁定 长按静音"},{"SP:LOCK LP:SOUND","短按锁定 长按开声"},
    {"RADIO NOT READY","蓝牙尚未就绪"},{"RPS: SEARCH NEARBY","寻找附近对手"},{"OPEN RPS ON PEER","对方也打开此游戏"},{"MATCHING...","正在匹配"},{"PEER LEFT / TIMEOUT","对方离开或超时"},{"SP: SEARCH AGAIN","短按重新寻找"},{"SP: NEXT MATCH","短按下一局"},{"CONFIRMING RESULT...","正在确认结果"},{"WAIT FOR PEER...","等待对方选择"},{"TURN:PICK SP:LOCK","转选出拳 短按锁定"},
    {"ROLLING","正在掷骰"},{"HIT POINT","命中目标点"},{"COME OUT","首次掷骰"},{"YOU WIN","你赢了"},{"YOU LOSE","你输了"},{"SP: AGAIN","短按再来"},{"SP: ROLL","短按掷骰"},{"CRAPS","花旗骰"},{"LUCK","好运"},{"DICE","骰子"},
    {"CLASSIC WALLS","经典边墙"},{"PILLARS","立柱"},{"GATES","关卡门"},{">CONTINUE",">继续上局"},{" CONTINUE"," 继续上局"},{">NEW GAME",">开始新局"},{" NEW GAME"," 开始新局"},{"SNAKE: SP START","短按开始"},{"GAME OVER","游戏结束"},{"YOU WIN!","你赢了"},{"TURN:MAP SP:START","转选图 按开始"},{"BACK:KEEP OLD SAVE","返回保留原存档"},{"SAVE ERR NEW ONLY","存档错 仅新局"},{"SP: NEW GAME","短按新局"},{"TURN: LEFT/RIGHT","旋转左右转向"},{"HOLD:NEW BACK:EXIT","长按新局 返回"},{"SAVE ERR HOLD:NEW","存错 长按新局"},
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
    {"Pet, arcade & party games","宠物与单人多人游戏"},
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
void ui_zh_draw_fit(u8g2_t *display,u8g2_int_t x,u8g2_int_t baseline,const char *text,unsigned width){
    const char *s=ui_zh_lookup(text);char fit[128];unsigned used=0;
    while(*s){
        unsigned lead=(unsigned char)*s;
        unsigned bytes=lead<128?1:(lead>=194 && lead<224?2:(lead<240?3:4));
        unsigned valid=1;while(valid<bytes && s[valid] && ((unsigned char)s[valid]&192)==128)valid++;
        if(valid!=bytes)break;
        if(used+bytes>=sizeof(fit))break;
        memcpy(fit+used,s,bytes);fit[used+bytes]=0;
        if(ui_zh_width(display,fit)>width)break;
        used+=bytes;s+=bytes;
    }
    fit[used]=0;ui_zh_draw(display,x,baseline,fit);
}
void ui_zh_three_line(u8g2_t *display,int x,int baseline,const char *text,unsigned width,unsigned now_ms){
    if(!ui_language_chinese()){u8g2_DrawStr(display,x,baseline,text);return;}
    if(baseline>=24){
        if(!(now_ms/3000%2))return;
        u8g2_SetDrawColor(display,0);u8g2_DrawBox(display,x,15,width,17);u8g2_SetDrawColor(display,1);
    }
    ui_zh_draw_fit(display,x,baseline<=12?11:27,text,width);
}
