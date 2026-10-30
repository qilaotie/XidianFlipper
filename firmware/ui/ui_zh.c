#include "ui_zh.h"
#include "ui_language.h"
#include <string.h>
extern const uint8_t halfstep_zh_font[];
typedef struct {const char *key,*value;} translation_t;
static const translation_t translations[]={
    {"- Gold:%u apple:%u snack:%u soap:%u","- 金币%u 苹果%u 零食%u 肥皂%u"},
    {"- Local send queue blocked","- 本机发送队列堵塞"},
    {"- Resume CH%u room:%08lX","- 继续需频道%u 队伍%08lX"},{"- Waiting CH%u room:%08lX","- 等待交付 频道%u 队伍%08lX"},
    {"- Network experiment","- 网络实验"},{"- Pet gifts","- 宠物交换礼物"},
    {"- Enable network first","- 请先开启自组网"},
    {"10 probes / 0 retries","十次探测 不重试"},{"10 probes / 2 retries","十次探测 最多重试两次"},{"30 probes / 1 retry","三十次探测 重试一次"},
    {"- Target: %.13s [%02X%02X]","- 目标：%.13s [%02X%02X]"},
    {"- Last [%02X%02X] ACK:%lu/%lu","- 上轮[%02X%02X] 确认%lu/%lu"},
    {"- Try:%lu retry:%lu timeout:%lu","- 发送%lu 重试%lu 超时%lu"},
    {"- RTT min/avg/max %lu/%lu/%lu ms","- 往返最小平均最大 %lu/%lu/%lu 毫秒"},
    {"- RTT: no confirmed reply","- 暂无确认回执"},{"- Stop experiment","- 停止实验"},{"- Start experiment","- 开始实验"},{"- Refresh direct neighbors","- 刷新直接邻居"},
    {"Select target / enable mesh","请选目标并开自组网"},
    {"GOLD","金币"},{"- Gift: %s x%u","- 礼物：%s 数量%u"},{"- Send gift: preview","- 赠送礼物：先预览"},
    {"- Accept incoming gift","- 确认接收礼物"},{"- Decline incoming gift","- 拒收礼物"},{"- Refresh gift peers","- 刷新礼物伙伴"},
    {"- From %.12s: %s x%u","- 来自%.12s：%s 数量%u"},{"- Accepted; waiting for commit","- 已接受 等待交付"},{"- Both open this gift page","- 双方打开礼物页面"},
    {"Gift blocked: funds/cap/storage","余额容量或存储不足"},{"- Confirm gift change","- 确认本次礼物操作"},{"- Peer [%02X%02X] ID:%lu","- 对方[%02X%02X] 编号%lu"},
    {"No gift","暂无礼物"},{"Escrow: awaiting accept","已托管 等待接收"},{"Committing: await receipt","交付中 等待回执"},{"Cancelling: await refund","撤回中 等待退款"},{"Gift delivered","礼物已送达"},{"Gift refunded","礼物已退回"},
    {"- Network topology demo","- 网络拓扑演示"},
    {"Mesh is off","自组网已关闭"},{"Tap opens network settings","短按打开网络设置"},
    {"No direct beacon heard","尚未收到邻居广播"},{"Tap: inspect message events","短按查看消息观测"},
    {"No recent routed events","暂无最近消息观测"},{"Tap: counters; hold: details","短按统计长按诊断"},
    {"Edges: direct beacons only","仅绘制邻居广播"},{"Relay path is unknown","中继路径未知"},
    {"- Pet and companion","- 宠物与陪伴"},{"- Solo games","- 单机小游戏"},{"- Nearby games","- 附近竞技"},
    {"- Group activities","- 协作活动"},{"- Network and connections","- 网络与连接"},
    {"Enable mesh in Settings","请在设置中开启自组网"},
    {"- Multiplayer tabletop","- 多人桌游"},
    {"- Remote controls","- 遥控器"},{"- Connections and cloud","- 联网与云同步"},{"- Scan and diagnostics","- 扫描与诊断"},
    {"- Display and language","- 显示与语言"},{"- Sound and lights","- 声音与灯光"},{"- Buttons and Morse","- 按键与摩斯"},{"- Power and system","- 电源与系统"},
    {"- Multiplayer check-in","- 多人签到"},
    {"PASSPORT","旅行护照"},
    {"- %08lX %s CH%u","- %08lX %s 频%u"},{"- Role %u: %s","- 声部%u：%s"},{"Melody","主旋律"},{"Bass","低音"},{"Beat","节拍"},{"Light only","仅灯光"},
    {"- Teams and social","- 队伍与社交"},{"- Team / room","- 队伍与房间"},{"- Relay mail cache","- 中继代存留言"},{"- Personal profile cards","- 个人资料卡"},{"- Pet travel passport","- 宠物旅行护照"},{"- Light and sound ensemble","- 多机灯光蜂鸣合奏"},
    {"- Room %s CH%u %08lX","- 房间%s 频道%u %08lX"},{"- Create room","- 创建队伍"},{"- Join 8-hex code","- 输入八位代码加入"},{"- Leave room","- 离开当前队伍"},{"- Resume saved room","- 重新加入已存队伍"},{"- Room group chat","- 进入队伍群聊"},{"Room code is not encryption","加入码不提供加密"},
    {"- Relay cache %s %u/4","- 代存%s 已占%u/4"},{"- Send relay mail from draft","- 用草稿发代存留言"},{"- Plaintext / 30 active minutes","- 明文 累计在线三十分钟"},{"- Slot%u %umin tries%u","- 槽%u 余%u分 已试%u次"},{"- Relay plaintext to %02X%02X","- 明文交中继转给%02X%02X"},{"Delete this relay copy?","删除本机代存副本？"},
    {"- My profile card","- 我的资料卡"},{"- Edit nickname","- 设置昵称"},{"- Edit signature","- 设置签名"},{"- Copy my current pet name","- 复制当前宠物名字"},{"- Share card: ON","- 资料卡分享：开"},{"- Share card: OFF","- 资料卡分享：关"},{"- Edit content","- 编辑当前内容"},{"Invalid length/code or SAVE ERROR","长度或代码无效 或保存失败"},
    {"- Nickname: %.12s","- 昵称：%.12s"},{"- Signature: %.24s","- 签名：%.24s"},{"- Pet name: %.12s","- 宠物名字：%.12s"},{"- Received card / may expire","- 收到的资料卡 会过期"},{"No travel stamps yet","还没有旅行印章"},{"Finish a pet visit to earn one","完成宠物拜访获得印章"},{"- Visits: %lu","- 拜访次数：%lu"},
    {"- Role: %u (melody/bass/beat/light)","- 声部%u 主旋律低音节拍灯光"},{"- Tune: %u","- 曲目：%u"},{"- Start 16-beat ensemble","- 开始十六拍合奏"},{"- Re-arm listening (60s)","- 重新监听六十秒"},{"Mute/LED settings apply; approximate sync","遵守静音灯光设置 近似同步"},{"- Listening / beat %u of 16","- 正在监听 第%u/16拍"},
    {"- Message management","- 消息管理"},{"- Multiplayer poll","- 多人投票"},{"- Team task board","- 组队任务板"},
    {"- Group check-in","- 多人签到"},{"- Quiz buzzer","- 联机抢答器"},{"- Activity history","- 活动记录"},{"- Network diagnostics","- 网络诊断"},
    {"- Local mesh network","- 自组网状态"},{"- Mesh group chat","- 多跳群聊"},{"- Network announcement","- 全网公告"},{"- Offline message","- 离线留言"},{"- Offline outbox","- 留言发件箱"},
    {"- All received","- 全部收到的消息"},{"- Unread received","- 未读消息"},{"- Current channel","- 当前频道"},{"- By friend","- 按好友筛选"},{"- Announcements","- 全网公告"},
    {"No matching messages","没有符合的消息"},{"BACK: FILTERS","返回筛选列表"},{"- Back to message","- 返回消息"},{"- Reply with draft","- 用草稿回复"},{"- Quote and reply","- 引用并回复"},{"- Delete this message","- 删除这条消息"},
    {"- Reply to channel %02u","- 回复到频道 %02u"},{"- Private reply %02u","- 私信回复 %02u"},{"- Private %02X%02X","- 私信 %02X%02X"},{"- Confirm send reply","- 确认发送回复"},
    {"Add sender as friend","请先加发送者为好友"},{"Quote + draft too long","引用加草稿超过上限"},{"Return to source channel","请切回原消息频道"},{"Friend no longer exists","对方已不在好友中"},
    {"- Keep message","- 保留消息"},{"- Confirm delete message","- 确认删除消息"},{"DELETE ERROR","删除失败"},
    {"- Contact status","- 联系人状态"},{"- Received from friend","- 查看对方的消息"},{"Directly heard <15s","直连可见 小于15秒"},{"Recently heard <60s","最近听到 小于60秒"},{"Status unknown","状态未知"},
    {"Direct RSSI %d dBm","直连信号 %d dBm"},{"%lus ago / %u hops","%lu秒前 听到%u跳"},{"Enable mesh to discover","开启自组网以发现设备"},{"Observed, not guaranteed","观测状态 不保证在线"},
    {"Enable mesh in Tools","请在工具中开启自组网"},{"RAM only / same channel","临时会话 需同频道"},{"Waiting for host ACK","等待主持端确认"},{"Host confirmed","主持端已确认"},{"Rejected / refresh view","操作被拒 请看最新状态"},{"No ACK: check host view","未获确认 请查看主持端"},
    {"POLL","投票"},{"CLOSED","已结束"},{"- Vote %c: %u","- 投 %c：%u票"},{"Host / voters","主持 已投"},{"Remote / voters","远端 已投"},{"Host stale / voters","主持未更新 已投"},
    {"- Close voting","- 结束投票"},{"- Create poll","- 创建投票"},{"Vote rejected / busy","投票被拒或操作正忙"},{"- Edit: %.48s","- 编辑：%.48s"},{"- Edit: %.32s","- 编辑：%.32s"},{"- Edit: %.20s","- 编辑：%.20s"},
    {"[EMPTY]","空白"},{"- Options: %u (A-D)","- 选项数：%u（A-D）"},{"- Title: max 18 ASCII","- 标题最多18个英文字符"},{"- Preview new poll","- 预览新投票"},{"- Preview new board","- 预览新任务板"},{"- Add task to my board","- 向我的板追加任务"},
    {"Need 1-48 ASCII chars","需要1至48个英文字符"},{"Need 1-18 ASCII chars","需要1至18个英文字符"},{"ASCII title required","标题请使用英文数字符号"},{"Close poll?","结束本次投票？"},{"- Confirm activity change","- 确认活动操作"},{"Mesh off / host busy","网络未开或主持端正忙"},{"Activity updated","活动已更新"},
    {"- My board (RAM only)","- 我的任务板 临时"},{"- Remote board (RAM only)","- 远端任务板 临时"},{"- Host stale / old board","- 主持未更新 旧任务板"},{"TAKEN","已领取"},{"OPEN","可领取"},{"EMPTY","空"},{"DONE","已完成"},
    {"- Create / add task","- 创建或追加任务"},{"Board ended","任务板已结束"},{"- Owner: %.13s","- 领取者：%.13s"},{"none","无"},{"- Title: %.18s","- 标题：%.18s"},{"- Title: %.48s","- 标题：%.48s"},
    {"- Claim task","- 领取任务"},{"- Mark completed","- 标记完成"},{"- Release task","- 释放任务"},{"Task rejected / busy","任务操作被拒或正忙"},
    {"- Send group CH%02u","- 群聊发送到频道%02u"},{"- Send ALL (max122)","- 发全网公告 最多122字符"},{"- Pick offline friend","- 选择留言好友"},{"- Network settings","- 自组网设置"},
    {"Announcement max122","公告最多122个字符"},{"No paired friends","还没有已配对好友"},{"Pair a friend first","请先添加好友"},{"- Confirm group CH%02u","- 确认发送到频道%02u"},{"- Queue for %02X%02X","- 留言给 %02X%02X"},{"- Confirm ALL channels","- 确认跨频道公告"},
    {"Outbox full / save error","发件箱已满或保存失败"},{"Queued; not delivered","已排队 尚未送达"},{"Enable mesh / busy","请开启自组网或稍后重试"},{"OUTBOX WAIT","待投递"},{"DELIVERED","已送达"},{"HELD","已暂停"},{"- %s TRY %u/%u","- %s 已试%u/%u次"},
    {"Outbox SAVE ERROR","发件箱保存错误"},{"BACK:EXIT / RETRY LATER","返回退出 稍后重试"},{"- Retry now","- 重新尝试"},{"- Delete / cancel","- 删除或取消"},{"- Enable mesh to deliver","- 开启自组网以投递"},{"- Confirm cancel/delete","- 确认取消并删除"},
    {"Mesh %s / max 3 hops","自组网%s 最多3跳"},{"Paused","暂停"},{"ON","开"},{"OFF","关"},{"CH%u direct peers:%u","频道%u 直连设备%u"},{"Forwarded %lu / heard %u hops","已转发%lu 听到%u跳"},{"Duplicate %lu / drop %lu","重复%lu 丢弃%lu"},{"Peer %u/%u / %lus ago","设备%u/%u %lu秒前"},
    {"Arrived","已到"},{"On the way","路上"},{"Not joining","不参加"},{"- Topic: %.32s","- 主题：%.32s"},{"- Host not heard recently","- 最近未听到主持端"},{"- Check-in open","- 正在签到"},{"- Buzzer OPEN","- 已开放抢答"},{"- Activity closed","- 活动已结束"},
    {"- I have arrived","- 我已到达"},{"- I am on the way","- 我在路上"},{"- I will not join","- 我不参加"},{"- BUZZ NOW","- 现在抢答"},{"- Winner: %.13s","- 抢答者：%.13s"},{"- Close check-in","- 结束签到"},{"- Cancel buzzer round","- 取消本轮抢答"},
    {"- Create check-in","- 发起多人签到"},{"- Open a new buzzer round","- 开放新一轮抢答"},{"Request rejected / busy","请求被拒或操作正忙"},{"- Title: max 32 ASCII","- 主题最多32个英文字符"},{"- Preview check-in","- 预览签到"},{"- Preview buzzer round","- 预览抢答"},{"Need 1-32 ASCII chars","需要1至32个英文字符"},{"Close this activity?","结束本次活动？"},
    {"- CH%u direct:%u heard:%u hops","- 频道%u 直连%u 最近%u跳"},{"- Forwarded:%lu dup:%lu","- 转发%lu 重复%lu"},{"- Route drop:%lu RX queue:%lu","- 转发丢弃%lu 接收队列丢弃%lu"},
    {"- Text sent:%lu receipts:%lu","- 文字发出%lu 回执%lu"},{"- Accepted:%lu rejected:%lu","- 接受%lu 拒绝%lu"},{"- Poll/task ACK:%lu ms","- 投票任务确认%lu毫秒"},{"- Check/buzz ACK:%lu ms","- 签到抢答确认%lu毫秒"},
    {"- ACKs:%lu retries:%lu","- 确认%lu 重发%lu"},{"- Timeouts:%lu records:%u","- 超时%lu 记录%u"},{"- History SAVE ERROR","- 活动记录保存失败"},{"Counters reset on reboot","诊断计数重启清零"},
    {"Poll ended","投票结束"},{"Task completed","任务完成"},{"Check-in ended","签到结束"},{"Buzzer ended","抢答结束"},{"History SAVE ERROR","活动记录保存失败"},{"No activity records","暂无活动记录"},{"Saved after activity ends","活动结束后自动记录"},{"Record not available","记录已不可用"},
    {"- Arrived:%u way:%u no:%u","- 已到%u 路上%u 不参加%u"},{"- Peer: %.13s","- 参与者：%.13s"},{"- Channel:%u sequence:%lu","- 频道%u 记录序号%lu"},{"- Saved at uptime:%lu s","- 保存时本次已开机%lu秒"},{"- Host:%02X%02X part:%u","- 主持%02X%02X 项目%u"},
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
    {"- Draft box","- 草稿箱"},{"- Edit saved draft","- 编辑已存草稿"},{"- Delete saved draft","- 删除已存草稿"},
    {"- Confirm load draft","- 确认替换当前消息"},{"- Confirm clear editor","- 确认清空编辑区"},
    {"- Confirm delete draft","- 确认删除草稿"},{"- Confirm overwrite draft","- 确认覆盖草稿"},
    {"Draft saved","草稿已保存"},{"Draft cleared","草稿已清空"},
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
