#include "help_text.h"
#include "ui_language.h"
#include "pet_help.h"
/* Chinese pages contain at most two 12px rows. English pet text stays intact. */
static const char *const pet_zh[9][5][2]={
 {{"接音符：转动换道","不用按键接住音符"},{"共落下十个音符","接住六个即可获胜"},{"左中右对应三条道","胜利奖励八枚金币"},{"开局耗八点精力","并耗三点饱食"}},
 {{"相扑：看力量变化","力量到七以上短按"},{"共推五次 赢三次","每次间隔至少半秒"},{"胜利奖励八枚金币","别在力量低时乱按"},{"开局耗八点精力","并耗三点饱食"}},
 {{"举旗：看旗子方向","左旗左转 右旗右转"},{"两面旗同时出现","请短按一次旋钮"},{"十次中答对六次","每次限一点二秒"},{"胜八币 耗八精力","另耗三点饱食"}},
 {{"顶球：转到球的道","最后三百毫秒短按"},{"共十个球 顶中六个","即可获得胜利"},{"你的位置对准目标","太早按也会顶空"},{"胜八币 耗八精力","另耗三点饱食"}},
 {{"记忆：看按键顺序","左转右转或短按"},{"等到轮到你再输入","按原顺序重复操作"},{"完成三轮即可获胜","每键五秒 总限二十秒"},{"胜八币 耗八精力","另耗三点饱食"}},
 {{"赛跑：快速短按","不要一直按住旋钮"},{"二十秒内短按三十次","即可获胜并得八币"},{"四十次会提前结束","至少一百一十毫秒"},{"开局耗八点精力","并耗三点饱食"}},
 {{"快速反应：先等待","等二点二到四秒信号"},{"出现开始信号后","一点二秒内短按"},{"抢按或超时无奖励","获胜得八枚金币"},{"耗八精力与三饱食","返回键可取消本局"}},
 {{"拜访：双方选拜访","等待三秒发现设备"},{"一方短按发出邀请","另一方短按同意"},{"双方各五币加礼物","每三分钟运行时间一次"},{"礼物是随机消耗品","背包满只发金币"}},
 {{"对战：双方选对战","发出邀请 等对方同意"},{"倒数三秒随机游戏","胜二十 平十 负六币"},{"相扑力量到七再按","顶球最后三百毫秒按"},{"赛跑快速短按计分","等双方确认结果领奖"},{"宠物醒着才可参加","耗八精力与三饱食"}}
};
unsigned ui_pet_help_pages(unsigned item){
    if(item>=9)return 0;
    if(!ui_language_chinese())return pet_help_pages(item);
    return item==PET_HELP_DUEL?5:4;
}
const char *ui_pet_help_line(unsigned item,unsigned page,unsigned row){
    if(item>=9 || page>=ui_pet_help_pages(item))return "";
    if(!ui_language_chinese())return pet_help_line(item,page,row);
    return row<2?pet_zh[item][page][row]:"";
}
typedef struct {const char *name;const char *en[4][2];const char *zh[4][2];} guide_t;
static const guide_t guides[10]={
 {"- Tetris portrait",{{"HOLD DEVICE PORTRAIT","10 COLUMNS / 20 ROWS"},{"TURN: MOVE; TAP: ROTATE","HOLD: DROP TO BOTTOM"},{"BACK: PAUSE / RESUME","PAUSE MENU: EXIT TO SAVE"},{"EACH LINE: PET +5G","CONTINUE RESTORES PAUSED"}},{{"方块：将设备竖持","十列二十行棋盘"},{"转动平移 短按旋转","长按直接落到底部"},{"返回键暂停或继续","暂停菜单选退出保存"},{"每消一行宠物得五币","续局后仍需确认开始"}}},
 {"- Snake",{{"TURN: RELATIVE LEFT/RIGHT","ONE TURN PER MOVEMENT"},{"TAP: START / PAUSE","HOLD: CHOOSE A NEW MAP"},{"WALL / BODY: GAME OVER","HOLLOW FOOD: GROW +1"},{"BACK: SAVE AND EXIT","ONE FOOD: PET +2G"}},{{"贪吃蛇：转动转向","每走一格只转一次"},{"短按开始或暂停","长按新局选择地图"},{"撞外墙障碍或身体结束","吃空心食物长一格"},{"返回保存并退出","每个食物宠物得两币"}}},
 {"- Answer book",{{"THINK OF A QUESTION","TAP TO GET AN ANSWER"},{"FOR FUN, NOT ADVICE","BACK: RETURN TO MENU"},{"NO NETWORK REQUIRED","NO COINS OR GAME SAVE"},{"TAP AGAIN: NEW ANSWER","BACK: EXIT"}},{{"答案之书：心想问题","短按查看随机答案"},{"仅供娱乐 不作建议","返回键回到菜单"},{"不需要联网或对手","不涉及金币或存档"},{"可再次短按换答案","随时返回退出"}}},
 {"- Dice",{{"TAP TO ROLL ONE DIE","RESULT: 1 TO 6"},{"TAP AGAIN TO REROLL","BACK: RETURN TO MENU"},{"NO NETWORK REQUIRED","NO COINS OR GAME SAVE"},{"FOR CASUAL TABLE GAMES","NOT FOR REAL MONEY"}},{{"普通骰子：短按掷骰","随机得到一到六点"},{"再次短按重新掷骰","返回键回到菜单"},{"不需要联网或对手","不涉及金币或存档"},{"可用于桌面小游戏","不要用于金钱赌博"}}},
 {"- Craps",{{"TAP TO ROLL TWO DICE","FIRST: 7/11 WINS"},{"FIRST: 2/3/12 LOSES","OTHER TOTAL SETS POINT"},{"ROLL POINT AGAIN: WIN","7 BEFORE POINT: LOSE"},{"TAP FOR THE NEXT ROUND","NO REAL-MONEY BETTING"}},{{"花旗骰：短按掷双骰","首掷七或十一获胜"},{"首掷二三十二失败","其他和成为目标点"},{"再次掷出目标点获胜","目标前掷出七则失败"},{"按提示开始下一轮","不涉及真实金钱下注"}}},
 {"- RPS nearby",{{"BOTH OPEN RPS NEARBY","WAIT FOR PEER MATCH"},{"TURN: ROCK/SCISSORS/PAPER","TAP: LOCK YOUR CHOICE"},{"BOTH LOCK: SHOW RESULT","CONFIRM FOR NEXT ROUND"},{"MATCH BY SIGNAL STRENGTH","NOT EXACT DISTANCE"}},{{"石头剪刀布：双方进入","等待匹配附近设备"},{"转动选择石头剪刀布","短按锁定自己的出拳"},{"双方锁定后公布结果","按提示确认下一轮"},{"按信号强弱近似匹配","不保证几何距离最近"}}},
 {"- Treasure radar",{{"ONE DEVICE HIDES","OTHERS SEARCH THE SAME"},{"TURN: HIDE / SEARCH","TAP: LOCK / UNLOCK TARGET"},{"HOLD: BEEP ON/OFF","STRONGER SIGNAL: FASTER"},{"NO DIRECTION OR METERS","GLOBAL BUZZER MUST BE ON"}},{{"寻宝：一台选择隐藏","其他设备进入寻找"},{"转动切换藏或找","短按锁定或解锁目标"},{"长按开关提示音","信号越强滴声越急"},{"不是方向或精确距离","全局声音开关需开启"}}},
 {"- Reaction party",{{"ONE HOST; OTHERS JOIN","2 TO 4 PLAYERS"},{"EVERYONE TAPS READY","NO NEW JOIN FOR 5s: START"},{"WAIT FOR GO, THEN TAP","EARLY TAP: FALSE START"},{"RESULTS SHOW TIME/RANK","ALL CONFIRM NEXT ROUND"}},{{"反应赛：一台创建房间","其余加入 共二到四人"},{"所有玩家短按准备","五秒无新人后自动开始"},{"等开始信号再短按","提前按视为抢跑"},{"结果显示用时与排名","所有人确认下一轮"}}},
 {"- Number bomb",{{"HOST A ROOM; OTHERS JOIN","2 TO 4 PLAYERS"},{"READY, THEN WAIT TO START","ONE HIDDEN NUMBER: 1-100"},{"TURN: CHOOSE; TAP: GUESS","ONLY CURRENT PLAYER ACTS"},{"HIT BOMB: LOSE THE ROUND","WATCH RANGE AND TIMEOUT"}},{{"数字炸弹：创建或加入","二到四人 准备后开局"},{"隐藏数字范围一到一百","玩家按轮次进行猜测"},{"转动选数 短按提交","只在轮到自己时操作"},{"猜中炸弹则本轮失败","注意范围缩小与超时"}}},
 {"- Time guess 1-10s",{{"HOST A ROOM; OTHERS JOIN","2 TO 4 PLAYERS"},{"READY; TARGET IS 1-10s","WATCH THE START SIGNAL"},{"COUNT SILENTLY, THEN TAP","ONE FINISH TAP PER PLAYER"},{"RANKED BY ABSOLUTE ERROR","ALL CONFIRM NEXT ROUND"}},{{"盲猜秒数：创建或加入","二到四人准备后开局"},{"目标随机一到十秒","看到开始信号后默数"},{"觉得时间到了就短按","每人只能提交一次"},{"按与目标的误差排名","所有人确认下一轮"}}}
};
static const guide_t pet_guides[4]={
 {"- Care and controls",{{"PET: TURN TO AN ACTION","TAP TO USE; HOLD: STATS"},{"FEED / CLEAN / SLEEP","WAKE BEFORE PLAYING"},{"FOOD MOOD REST CLEAN","NEEDS CAN TRIGGER CALLS"},{"BACK: RETURN TO MENU","CARE NEEDS NO GOLD"}},{{"宠物：转动选择操作","短按执行 长按看属性"},{"可喂食清洁睡觉唤醒","玩游戏前先让宠物醒来"},{"关注饱食心情精力清洁","需要照顾时会发出提醒"},{"返回键退出宠物页面","基础照顾不需要金币"}}},
 {"- Growth and naming",{{"EGG BABY TEEN ADULT","AGE COUNTS RUNNING TIME"},{"CARE/TRAINING AFFECT FORM","ADULT FORM STAYS FIXED"},{"NAME: UP TO 12 ASCII","HOLD OR SAVE TO CONFIRM"},{"DEX: CHECK FOUND FORMS","NEW EGG KEEPS COLLECTION"}},{{"蛋幼体少年成年四阶段","年龄按运行时间累积"},{"照顾训练影响成年形态","成年后形态不再改变"},{"名字限十二个英文字符","改名页保存或长按确认"},{"图鉴可查看已发现形态","重新养蛋保留收集金币"}}},
 {"- Shop and rewards",{{"SHOP: TAP TO BUY","BAG: USE / EQUIP ITEMS"},{"GAMES WIN: EARN GOLD","TETRIS LINE +5 SNAKE +2"},{"VISIT: BOTH GET GIFTS","DUEL: WIN DRAW LOSS PAY"},{"OWNED DECOR EQUIPS FREE","NEW EGG KEEPS INVENTORY"}},{{"商店短按购买背包使用","永久装饰装备不再扣币"},{"宠物小游戏获胜得八币","方块行五币蛇吃食两币"},{"拜访双方都有金币礼物","对战胜平负均有金币"},{"道具可补充不同属性","新蛋保留背包和钱包"}}},
 {"- Rooms and idle actions",{{"ROOM: TURN TO PREVIEW","TAP: BUY AND EQUIP"},{"CLASSIC FREE STUDY 25G","GARDEN 35G SPACE 50G"},{"OWNED ROOMS EQUIP FREE","ROOM + DECOR CAN COMBINE"},{"IDLE: WAIT TO SEE ACTIONS","SLEEP/LOW REST: CALM"}},{{"房间转动预览短按装备","未购买时扣币解锁"},{"原房免费 书房25币","花园35币 太空50币"},{"房间买过后免费切换","背景与原有装饰叠加"},{"停留可看哈欠挠头动作","睡觉精力低时动作安静"}}}
};
const char *pet_guide_name(unsigned item){
    static const char *const names[]={"- Catch notes","- Bump","- Flags","- Heading","- Memory","- Sprint","- Quick tap","- Visit","- Duel"};
    return item<4?pet_guides[item].name:(item<PET_GUIDES?names[item-4]:"");
}
unsigned pet_guide_pages(unsigned item){return item<4?4:(item<PET_GUIDES?(ui_language_chinese()?ui_pet_help_pages(item-4):(pet_help_pages(item-4)*3+1)/2):0);}
const char *pet_guide_line(unsigned item,unsigned page,unsigned row){
    if(item>=PET_GUIDES || page>=pet_guide_pages(item) || row>=2)return "";
    if(item<4)return ui_language_chinese()?pet_guides[item].zh[page][row]:pet_guides[item].en[page][row];
    /* English pet pages have three original rows; expose all rows over pairs. */
    if(!ui_language_chinese()){unsigned flat=page*2+row;return flat<pet_help_pages(item-4)*3?pet_help_line(item-4,flat/3,flat%3):"";}
    return ui_pet_help_line(item-4,page,row);
}
const char *game_guide_name(unsigned item){return item==11?"- Multiplayer tabletop":item==10?"- Pet":(item<10?guides[item].name:"");}
unsigned game_guide_pages(unsigned item){return item<GAME_GUIDES?4:0;}
const char *game_guide_line(unsigned item,unsigned page,unsigned row){
    if(item>=GAME_GUIDES || page>=4 || row>=2)return "";
    if(item==10)return ui_language_chinese()?pet_guides[0].zh[page][row]:pet_guides[0].en[page][row];
    if(item==11){
        static const char *const en[4][2]={{"FUN > TABLETOP","10 GAMES; PICK A TABLE"},{"SAME CHANNEL / ROOM","CREATE OR CHOOSE TABLE"},{"ALL READY; HOST START","PRIVATE INFO: HOLD"},{"TURN:PICK TAP:USE","INVITES / WATCH"}};
        static const char *const zh[4][2]={{"趣玩进入多人桌游","十款玩法多桌并行"},{"同频道与队伍范围","打开自组网后选桌"},{"全员准备后桌主开始","秘密信息长按查看"},{"旋转选择短按操作","可接邀请或公开观战"}};
        return ui_language_chinese()?zh[page][row]:en[page][row];
    }
    return ui_language_chinese()?guides[item].zh[page][row]:guides[item].en[page][row];
}
