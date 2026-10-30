#include "party_pages.h"
#include "wououi.h"
#include "party_link.h"
#include "message_service.h"
#include "ui_language.h"
#include "ui_zh.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define T(en,zh) (ui_language_chinese()?(zh):(en))
static party_snapshot_t v;
static unsigned selected_game,action_at,param_a,param_b,param_c,view_mode,help_page,cell,liar_mask;
static bool private_open,list_enter,initial_a_set;
static bool board_opened;
static uint32_t board_revision;
static void graphic_reset(void);
static void graphic_hide(void);
static void graphic_game(void);
static void adapt_reset(void);
static unsigned initial_a,initial_b;
static uint32_t seen_id,seen_boot;
static uint8_t seen_host[6];
static bool table_browser,table_detail;
static unsigned table_filter=TABLETOP_GAMES,table_count;
static party_table_t table_rows[PARTY_TABLES];
static party_table_t table_choice;
static unsigned table_return_row;
static uint8_t seen_result;
static uint8_t seen_round;
static unsigned edit_param;
static unsigned help_total,help_offsets[20];
static unsigned help_render_page=~0u;
static char help_render_lines[2][96];
static char error_text[96];static int64_t error_until;
static const char *const game_en[]={"UNO","Undercover","Da Vinci Code","Liar's tavern","Love Letter","Yacht dice","Avalon","One Night","Werewolf","Tic-tac-toe","Reversi"};
static const char *const game_zh[]={"UNO变色牌","谁是卧底","达芬奇密码","骗子酒馆","情书","快艇骰子","阿瓦隆","一夜狼","狼人杀","井字棋","黑白棋"};
static const char *game_name(unsigned g){return g<TABLETOP_GAMES?(ui_language_chinese()?game_zh[g]:game_en[g]):"?";}
static void back(void){btn.pressed=false;if(ui.layer){ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}}
static void notice(const char *s){snprintf(error_text,sizeof(error_text),"%s",s);error_until=esp_timer_get_time()+2000000;}
static int menu(M_SELECT *rows,unsigned n){
    ui.num[ui.index]=n;if(ui.select[ui.layer]>=n)ui.select[ui.layer]=0;
    if(list_enter){list_enter=false;list_update_scroll_targets();list.y=list.y_trg;list.box_y=list.box_y_trg[ui.layer];list.loop=false;ui.init=true;}
    list_show(rows,ui.index);
    if(!btn.pressed)return -1;
    if(btn.id==BTN_ID_CW || btn.id==BTN_ID_CC){btn.pressed=false;list_rotate_switch();}
    else if(btn.id==BTN_ID_SP){btn.pressed=false;return ui.select[ui.layer];}
    return -1;
}
static void line(unsigned y,const char *s){u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);ui_zh_draw_fit(&u8g2,1,y,s,126);}
static bool request(unsigned op,unsigned action,unsigned a,unsigned b,unsigned c){
    bool ok=message_service_party_request(op,action,a,b,c);
    if(!ok)notice(T("Invalid / busy / sync first","条件不符或正在同步"));
    else if(!v.local)notice(T("Sent; wait for host ACK","已提交 等待主机确认"));
    return ok;
}
static const char *phase_name(unsigned p){
    switch(p){case PG_LOBBY:return T("Lobby","等待加入");case PG_PLAYING:return T("Playing","进行中");case PG_DISCUSS:return T("Discuss","自由讨论");case PG_VOTING:return T("Vote","正在投票");case PG_NOMINATE:return T("Pick team","队长组队");case PG_MISSION:return T("Mission","执行任务");case PG_ASSASSIN:return T("Assassinate","刺客指认");case PG_NIGHT_WOLF:return T("Wolves night","狼人行动");case PG_NIGHT_SEER:return T("Seer night","预言家行动");case PG_NIGHT_ROBBER:return T("Robber night","强盗行动");case PG_NIGHT_SWAP:return T("Swap night","捣蛋鬼行动");case PG_NIGHT_WITCH:return T("Witch night","女巫行动");case PG_HUNTER:return T("Hunter shot","猎人开枪");case PG_FINISHED:return T("Finished","本局结束");default:return "?";}
}
static const char *role_name(unsigned role,unsigned kind){
    if(kind==PG_UNDERCOVER)return role==1?T("Undercover","卧底"):T("Civilian","平民");
    if(kind==PG_AVALON){static const char *en[]={"Loyal","Merlin","Assassin","Minion"},*zh[]={"忠臣","梅林","刺客","爪牙"};return role<4?(ui_language_chinese()?zh[role]:en[role]):"?";}
    if(kind==PG_NIGHT){static const char *en[]={"Villager","Wolf","Seer","Robber","Troublemaker"},*zh[]={"村民","狼人","预言家","强盗","捣蛋鬼"};return role<5?(ui_language_chinese()?zh[role]:en[role]):"?";}
    static const char *en[]={"Villager","Wolf","Seer","Witch","Hunter"},*zh[]={"村民","狼人","预言家","女巫","猎人"};return role<5?(ui_language_chinese()?zh[role]:en[role]):"?";
}
static void seat_name(unsigned p,char out[32]){if(p>=v.game.n){snprintf(out,32,"--");return;}snprintf(out,32,"P%u %.6s%s",p+1,v.names[p],p==v.game.me?T(" (me)"," 我"):"");}
static void card_name(unsigned kind,unsigned card,char out[48]){
    if(kind==PG_UNO){static const char *r[]={"0","1","2","3","4","5","6","7","8","9","SKIP","REV","+2"};if(card>=52)snprintf(out,48,"%s",card==52?"WILD":"WILD +4");else snprintf(out,48,"%c %s",'A'+card/13,r[card%13]);}
    else if(kind==PG_CODE)snprintf(out,48,"%c%u%s",(card&127)&1?'W':'B',(card&127)/2,card&128?" OPEN":"");
    else if(kind==PG_LOVE){static const char *en[]={"?","Guard","Priest","Baron","Handmaid","Prince","King","Countess","Princess"},*zh[]={"?","卫兵","牧师","男爵","侍女","王子","国王","伯爵夫人","公主"};snprintf(out,48,"%u %s",card,card<=8?(ui_language_chinese()?zh[card]:en[card]):"?");}
    else snprintf(out,48,"%s%u",kind==PG_LIAR?(card?"R":"JOKER "):"",card);
}
static const char *const category_en[]={"Ones","Twos","Threes","Fours","Fives","Sixes","3 of a kind","4 of a kind","Full house","Small straight","Large straight","Yacht","Chance"};
static const char *const category_zh[]={"一点","二点","三点","四点","五点","六点","三同号","四同号","葫芦","小顺子","大顺子","五同号","全选"};
static const char *category(unsigned i){return i<13?(ui_language_chinese()?category_zh[i]:category_en[i]):"?";}
/* Compact local word pairs; only a player's own word is exposed in their view. */
static const char *const words[24][2]={{"苹果","梨子"},{"牛奶","豆浆"},{"火车","地铁"},{"猫咪","小狗"},{"太阳","月亮"},{"雨伞","雨衣"},{"足球","篮球"},{"老师","学生"},{"冰箱","空调"},{"手机","电脑"},{"面包","蛋糕"},{"海洋","湖泊"},{"铅笔","钢笔"},{"电影","电视"},{"饺子","包子"},{"眼镜","墨镜"},{"白糖","食盐"},{"蝴蝶","蜜蜂"},{"雪糕","冰块"},{"飞机","轮船"},{"沙发","椅子"},{"围巾","手套"},{"茶叶","咖啡"},{"书店","图书馆"}};
static const char *const words_en[24][2]={{"Apple","Pear"},{"Milk","Soy milk"},{"Train","Subway"},{"Cat","Dog"},{"Sun","Moon"},{"Umbrella","Raincoat"},{"Football","Basketball"},{"Teacher","Student"},{"Fridge","Aircon"},{"Phone","Computer"},{"Bread","Cake"},{"Ocean","Lake"},{"Pencil","Pen"},{"Movie","TV"},{"Dumpling","Bun"},{"Glasses","Sunglasses"},{"Sugar","Salt"},{"Butterfly","Bee"},{"Ice cream","Ice cube"},{"Plane","Ship"},{"Sofa","Chair"},{"Scarf","Gloves"},{"Tea","Coffee"},{"Bookshop","Library"}};
void party_pages_enter(uint8_t page){
    if(page<M_PARTY_HUB || page>M_PARTY_LEAVE)return;
    list_enter=true;
    if(page==M_PARTY_VIEW){graphic_hide();adapt_reset();private_open=false;if(v.game.kind!=PG_TIC)cell=0;}
    if(page==M_PARTY_ACTION){action_at=0;param_a=initial_a_set?initial_a:0;param_b=initial_a_set?initial_b:0;initial_a_set=false;initial_b=0;param_c=v.game.kind==PG_CODE?0:2;edit_param=0;}
    if(page==M_PARTY_HELP){help_page=0;help_total=0;help_render_page=~0u;}
}
static void hub(void){
    static M_SELECT rows[TABLETOP_GAMES+4];static char names[TABLETOP_GAMES][80];
    rows[0].m_select=(char *)T(v.enabled?"- Network: ON (toggle)":"- Network: OFF (toggle)",v.enabled?"- 自组网：开 点击切换":"- 自组网：关 点击切换");rows[1].m_select=(char *)T("- My current game table","- 我的当前游戏桌");
    unsigned count=2,map[TABLETOP_GAMES];
    for(unsigned i=0;i<TABLETOP_GAMES;i++)if(party_game_available(i)){map[count-2]=i;snprintf(names[i],80,"- %s (%u-%u)",game_name(i),party_min(i),party_max(i));rows[count++].m_select=names[i];}
    rows[count].m_select=(char *)T("- Nearby game tables","- 附近游戏桌");
    unsigned invitations=0;for(unsigned i=0;i<PARTY_TABLES;i++)if(v.tables[i].active && v.tables[i].invited && v.tables[i].phase==PG_LOBBY)invitations++;
    char invite_label[80];snprintf(invite_label,sizeof(invite_label),T("- Game invitations (%u)","- 游戏邀请（%u）"),invitations);rows[count+1].m_select=invite_label;
    int n=menu(rows,count+2);if(n==0)message_service_network_enable(!v.enabled);
    if(n==(int)count || n==(int)count+1 || (n==1 && !v.active)){table_browser=true;table_detail=false;table_filter=n==(int)count+1?TABLETOP_GAMES+1:TABLETOP_GAMES;table_count=0;for(unsigned i=0;i<PARTY_TABLES;i++)if(v.tables[i].active && (table_filter==TABLETOP_GAMES || (v.tables[i].invited && v.tables[i].phase==PG_LOBBY)))table_rows[table_count++]=v.tables[i];ui_navigate_to(M_PARTY_CREATE);}
    else if(n==1)ui_navigate_to(M_PARTY_ROOM);
    else if(n>=2){table_browser=false;selected_game=map[n-2];ui_navigate_to(M_PARTY_CREATE);}
}
static void tables_refresh(void){table_detail=false;table_count=0;for(unsigned i=0;i<PARTY_TABLES;i++)if(v.tables[i].active && (table_filter==TABLETOP_GAMES || v.tables[i].kind==table_filter || (table_filter==TABLETOP_GAMES+1 && v.tables[i].invited && v.tables[i].phase==PG_LOBBY)))table_rows[table_count++]=v.tables[i];ui.select[ui.layer]=0;list_enter=true;}
static void tables_back(void){table_browser=false;if(table_filter>=TABLETOP_GAMES)back();else{ui.select[ui.layer]=0;list_enter=true;}}
static void table_detail_back(void){table_detail=false;ui.select[ui.layer]=table_return_row;list_enter=true;}
static void table_info(void){
    if(btn.pressed && btn.id==BTN_ID_BACK){btn.pressed=false;table_detail_back();return;}
    bool found=false;
    for(unsigned i=0;i<PARTY_TABLES;i++){const party_table_t *t=&v.tables[i];if(t->active && t->id==table_choice.id && t->host_boot==table_choice.host_boot && !memcmp(t->host,table_choice.host,6)){table_choice=*t;found=true;break;}}
    char title[80],host[64],state[80],id[48];
    snprintf(title,sizeof(title),"- %s",game_name(table_choice.kind));
    snprintf(host,sizeof(host),"- %s %.6s [%02X%02X]",T("Host","桌主"),table_choice.name[0]?table_choice.name:"?",table_choice.host[4],table_choice.host[5]);
    snprintf(state,sizeof(state),"- %u/%u %s",table_choice.n,party_max(table_choice.kind),found?phase_name(table_choice.phase):T("Offline","已离线"));
    snprintf(id,sizeof(id),"- %s %lu",T("Table","游戏桌"),(unsigned long)table_choice.id);
    M_SELECT rows[]={{title},{host},{state},{id},{(char *)T("- Join / restore my seat","- 加入或恢复我的座位")},{(char *)T("- Back to table list","- 返回游戏桌列表")},{(char *)T("- Watch public table","- 观战公开桌面")}};
    int n=menu(rows,COUNT(rows));if(n==5)table_detail_back();
    if(n==4){if(found && message_service_party_join_table(table_choice.host,table_choice.host_boot,table_choice.id)){table_browser=table_detail=false;liar_mask=0;ui_navigate_to(M_PARTY_ROOM);}else notice(T("End current game / refresh tables","请结束当前对局或刷新列表"));}
    if(n==6){if(found && message_service_party_watch_table(table_choice.host,table_choice.host_boot,table_choice.id)){table_browser=table_detail=false;liar_mask=0;ui_navigate_to(M_PARTY_ROOM);}else notice(T("End current game / refresh tables","请结束当前对局或刷新列表"));}
}
static void tables(void){
    if(table_detail){table_info();return;}
    if(btn.pressed && btn.id==BTN_ID_BACK){btn.pressed=false;tables_back();return;}
    static M_SELECT rows[PARTY_TABLES+3];static char labels[PARTY_TABLES][128];
    rows[0].m_select=(char *)T("- Refresh table list","- 刷新游戏桌列表");rows[1].m_select=(char *)T("- Back","- 返回");
    for(unsigned i=0;i<table_count;i++){
        const party_table_t *t=&table_rows[i];
        snprintf(labels[i],sizeof(labels[i]),"- %s %.6s [%02X%02X/%lu] %u/%u %s",game_name(t->kind),t->name[0]?t->name:"?",t->host[4],t->host[5],(unsigned long)t->id,t->n,party_max(t->kind),phase_name(t->phase));rows[i+2].m_select=labels[i];
    }
    if(!table_count)rows[2].m_select=(char *)T(v.enabled?"- No table found; refresh later":"- Enable network first",v.enabled?"- 未发现游戏桌 请稍后刷新":"- 请先打开自组网");
    int n=menu(rows,table_count?table_count+2:3);
    if(n==0)tables_refresh();else if(n==1)tables_back();
    else if(n>=2 && (unsigned)n-2<table_count){table_choice=table_rows[n-2];table_return_row=n;table_detail=true;ui.select[ui.layer]=0;list_enter=true;}
}
static void create(void){
    if(table_browser){tables();return;}
    char title[80];snprintf(title,sizeof(title),"- %s %u-%u",game_name(selected_game),party_min(selected_game),party_max(selected_game));
    M_SELECT rows[]={{title},{(char *)T("- Rules and controls","- 规则与操作")},{(char *)T("- Create as host","- 创建并担任主机")},{(char *)T("- Choose a table to join","- 选择游戏桌加入")}};
    int n=menu(rows,COUNT(rows));if(n==1)ui_navigate_to(M_PARTY_HELP);
    if(n==2){if(message_service_party_create(selected_game)){liar_mask=0;ui_navigate_to(M_PARTY_ROOM);}else notice(T("Enable mesh / end my current game","请开自组网或结束旧局"));}
    if(n==3){table_browser=true;table_filter=selected_game;tables_refresh();}
}
static void room(void){
    if(!v.active){line(11,T("No room / scope changed","房间已失效或范围变化"));line(27,T("BACK to choose a game","返回选择游戏"));return;}
    if(party_game_graphical(v.game.kind) && v.game.phase==PG_PLAYING && (v.game.me<v.game.n || v.watching) && !board_opened){btn.pressed=false;board_opened=true;view_mode=1;ui_navigate_to(M_PARTY_VIEW);return;}
    char title[96],state[96];snprintf(title,sizeof(title),"- %s %u/%u",game_name(v.game.kind),v.game.n,party_max(v.game.kind));
    snprintf(state,sizeof(state),"- %s P%u R%u%s",phase_name(v.game.phase),v.game.turn+1,v.game.round,v.stale?T(" STALE"," 主机离线"):"");
    bool member=v.local || v.game.me<v.game.n;
    M_SELECT rows[10]={{title},{state},{(char *)T(!member?"- Join / restore my seat":v.game.phase==PG_LOBBY?"- Host: start match":"- Player list / scores",!member?"- 加入或恢复我的座位":v.game.phase==PG_LOBBY?"- 主机开始游戏":"- 玩家名单与分数")},{(char *)T("- My cards / secret role","- 我的手牌与身份")},{(char *)T("- Public table","- 公共桌面")},{(char *)T("- Take an action","- 执行操作")},{(char *)T("- Rules and controls","- 规则与操作")},{(char *)T("- Leave / end match","- 离开或结束本局")}};
    char piece[48];if(v.game.kind==PG_TIC){snprintf(piece,sizeof(piece),T("- My piece: %c","- 我的棋子：%c"),v.game.me==0?'X':v.game.me==1?'O':'-');rows[3].m_select=piece;rows[4].m_select=(char *)T("- Play on the board","- 棋盘对战");rows[5].m_select=(char *)T("- Rotate to pick; tap to play","- 旋转选格 短按落子");}
    if(v.game.kind==PG_UNO || v.game.kind==PG_YACHT){rows[3].m_select=(char *)T("- Open graphical game","- 打开图形对局");rows[4].m_select=rows[3].m_select;rows[5].m_select=rows[3].m_select;}
    unsigned count=8;char ready_label[80];
    if(v.watching){rows[2].m_select=(char *)T("- Watching: player list","- 观战：玩家名单");rows[3].m_select=rows[5].m_select=(char *)T("- Public information only","- 仅查看公开信息");rows[7].m_select=(char *)T("- Stop watching","- 退出观战");}
    else if(member && v.game.phase==PG_LOBBY){unsigned ready_count=0;for(unsigned i=0;i<v.game.n;i++)ready_count+=(v.ready_mask>>i)&1;snprintf(state,sizeof(state),T("- Ready %u/%u; host starts","- 已准备 %u/%u 桌主开局"),ready_count,v.game.n);snprintf(ready_label,sizeof(ready_label),T("- My ready: %s (toggle)","- 我的准备：%s 点击切换"),T(v.ready_mask&(1u<<v.game.me)?"YES":"NO",v.ready_mask&(1u<<v.game.me)?"是":"否"));rows[count++].m_select=ready_label;if(v.local)rows[count++].m_select=(char *)T("- Invite players in this room","- 邀请本队伍玩家");}
    int n=menu(rows,count);selected_game=v.game.kind;
    if(n==8 && !v.watching && member && v.game.phase==PG_LOBBY)request(PARTY_READY,0,!(v.ready_mask&(1u<<v.game.me)),0,0);
    if(n==9 && v.local && v.game.phase==PG_LOBBY){if(request(PARTY_INVITE,0,0,0,0))notice(T("Invitation queued","邀请已排队"));}
    if(n==2){if(v.watching){view_mode=2;ui_navigate_to(M_PARTY_VIEW);}else if(!member)request(PARTY_JOIN,0,0,0,0);else if(v.game.phase==PG_LOBBY && v.local){if(v.ready_mask!=((1u<<v.game.n)-1))notice(T("Wait until all players ready","请等待所有玩家准备"));else request(PARTY_START,0,0,0,0);}else{view_mode=2;ui_navigate_to(M_PARTY_VIEW);}}
    if(n==3 && v.game.kind!=PG_TIC){view_mode=(party_game_graphical(v.game.kind) || v.watching)?1:0;ui_navigate_to(M_PARTY_VIEW);}
    if(n==4){view_mode=1;ui_navigate_to(M_PARTY_VIEW);}
    if(n==5){if(v.watching){view_mode=1;ui_navigate_to(M_PARTY_VIEW);}else if(v.game.kind==PG_TIC || v.game.kind==PG_CODE || v.game.kind==PG_LIAR || v.game.kind==PG_LOVE){view_mode=(v.game.kind==PG_TIC || v.game.kind==PG_CODE)?1:0;ui_navigate_to(M_PARTY_VIEW);}else if(!member || !v.ready || v.stale || v.pending)notice(T("Join / wait for fresh state","先加入并等待同步"));else ui_navigate_to(M_PARTY_ACTION);}
    if(n==6)ui_navigate_to(M_PARTY_HELP);
    if(n==7){if(v.watching){message_service_party_stop_watching();back();}else ui_navigate_to(M_PARTY_LEAVE);}
}
static void board(void){
    if(v.game.phase!=PG_LOBBY)board_opened=true;
    cell%=9;
    if(board_revision!=v.revision){board_revision=v.revision;if(v.game.phase==PG_PLAYING && v.game.board[cell])for(unsigned i=1;i<=9;i++)if(!v.game.board[(cell+i)%9]){cell=(cell+i)%9;break;}}
    if(btn.pressed && (btn.id==BTN_ID_CW || btn.id==BTN_ID_CC)){int step=btn.id==BTN_ID_CW?1:-1;btn.pressed=false;for(unsigned i=0;i<9;i++){cell=(cell+9+step)%9;if(!v.game.board[cell] || v.game.phase==PG_FINISHED)break;}}
    if(btn.pressed && btn.id==BTN_ID_SP){btn.pressed=false;if(v.game.phase==PG_PLAYING && v.enabled && v.ready && !v.stale && !v.pending && v.game.me==v.game.turn && !v.game.board[cell])request(PARTY_MOVE,PG_CARD,cell,0,0);}
    u8g2_SetDrawColor(&u8g2,1);
    for(unsigned i=0;i<4;i++){u8g2_DrawVLine(&u8g2,i*10,0,31);u8g2_DrawHLine(&u8g2,0,i*10,31);}
    for(unsigned i=0;i<9;i++){
        int x=1+(i%3)*10,y=1+(i/3)*10;bool cursor=i==cell && v.game.phase==PG_PLAYING && v.game.me<v.game.n;
        if(cursor){u8g2_SetDrawColor(&u8g2,1);u8g2_DrawBox(&u8g2,x,y,9,9);u8g2_SetDrawColor(&u8g2,0);}else u8g2_SetDrawColor(&u8g2,1);
        if(v.game.board[i]==1){u8g2_DrawLine(&u8g2,x+1,y+1,x+7,y+7);u8g2_DrawLine(&u8g2,x+7,y+1,x+1,y+7);}
        if(v.game.board[i]==2)u8g2_DrawCircle(&u8g2,x+4,y+4,3,U8G2_DRAW_ALL);
    }
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);char label[24];snprintf(label,sizeof(label),"P%u:%c  %c%u",v.game.me<2?v.game.me+1:0,v.game.me==0?'X':v.game.me==1?'O':'-','A'+cell%3,cell/3+1);if(v.watching)snprintf(label,sizeof(label),"WATCH %c%u",'A'+cell%3,cell/3+1);u8g2_DrawStr(&u8g2,38,7,label);
    const char *state;
    if(v.watching)state=T("SPECTATING","正在观战");
    else if(v.game.phase==PG_FINISHED)state=v.game.winner==3?T("DRAW","平局"):!v.game.winner?T("ENDED","本局结束"):v.game.me<8 && (v.game.winner&(1u<<v.game.me))?T("YOU WIN","你赢了"):T("YOU LOSE","对方获胜");
    else if(v.game.phase!=PG_PLAYING)state=T("WAIT START","等待开局");else if(!v.enabled)state=T("NETWORK OFF","网络关闭");else if(v.stale)state=T("HOST OFFLINE","主机离线");else if(v.pending || !v.ready)state=T("SYNCING","同步中");else if(v.game.me==v.game.turn)state=T("YOUR TURN","轮到你下");else state=T("WAIT OPPONENT","等待对方");
    ui_zh_draw_fit(&u8g2,38,21,state,89);u8g2_DrawStr(&u8g2,38,31,v.game.phase==PG_FINISHED?"BACK:ROOM":"TURN:PICK TAP:GO");
}
#include "party_graphics.inc"
#include "party_adapt.inc"
static void view(void){
    if(v.watching && view_mode==0)view_mode=1;
    if(!v.active){line(20,T("No room","没有房间"));return;}
    if(view_mode!=2 && party_game_graphical(v.game.kind)){graphic_game();return;}
    if(view_mode==0 && v.game.phase==PG_LOBBY){line(11,T("Wait for host to start","等待主机开始"));return;}
    if(view_mode==0 && (!v.ready || v.game.me>=v.game.n)){line(11,T("Join / synchronizing","请加入或等待同步"));return;}
    if(view_mode==0 && !private_open){
        line(11,T("Private: shield your screen","私密页面 请遮挡屏幕"));line(27,T("HOLD to reveal; BACK exits","长按查看 返回退出"));
        if(btn.pressed && btn.id==BTN_ID_LP){private_open=true;btn.pressed=false;list_enter=true;}return;
    }
    if(view_mode==1 && v.game.kind==PG_TIC){board();return;}
    if(view_mode==0 && (v.game.kind==PG_CODE || v.game.kind==PG_LIAR || v.game.kind==PG_LOVE)){adapt_hand();return;}
    if(view_mode==1 && v.game.kind==PG_CODE){adapt_code();return;}
    if(view_mode==1 && (v.game.kind==PG_UNDERCOVER || v.game.kind==PG_AVALON || v.game.kind==PG_NIGHT || v.game.kind==PG_WOLF)){adapt_players();return;}
    static M_SELECT rows[40];static char labels[40][96];unsigned count=0;
#define ROW(...) do{snprintf(labels[count],96,__VA_ARGS__);rows[count].m_select=labels[count];count++;}while(0)
    if(view_mode==2){for(unsigned p=0;p<v.game.n;p++){char name[32];seat_name(p,name);if(v.game.phase==PG_LOBBY)ROW("- %s %s",name,T(v.ready_mask&(1u<<p)?"READY":"WAIT",v.ready_mask&(1u<<p)?"已准备":"未准备"));else ROW("- %s %u %s%s",name,v.game.score[p],v.game.flags[p]&PG_ALIVE?"":"OUT ",v.game.winner&(1u<<p)?"WIN":"");if(v.game.phase==PG_FINISHED && (v.game.kind==PG_UNDERCOVER || v.game.kind==PG_AVALON || v.game.kind==PG_NIGHT || v.game.kind==PG_WOLF))ROW("- %s",role_name(v.game.board[p],v.game.kind));}if(v.game.phase==PG_FINISHED)ROW("- %s",T("Match complete; create another","本局结束 可创建新局"));}
    else if(view_mode==0){
        if(v.game.kind==PG_UNDERCOVER){unsigned i=v.game.extra[0]%24,which=v.game.extra[1]&1;ROW("- %s",ui_language_chinese()?words[i][which]:words_en[i][which]);ROW("- %s",T("Describe aloud; do not show word","轮流描述 不要展示词语"));}
        else if(v.game.kind==PG_AVALON || v.game.kind==PG_NIGHT || v.game.kind==PG_WOLF){
            ROW("- %s: %s",T("My role","我的身份"),role_name(v.game.role,v.game.kind));
            if(v.game.kind==PG_AVALON && v.game.role)for(unsigned p=0;p<v.game.n;p++)if(v.game.extra[0]&(1u<<p))ROW("- %s P%u",T("Known evil","已知坏人"),p+1);
            if((v.game.kind==PG_NIGHT || v.game.kind==PG_WOLF) && v.game.role==1)for(unsigned p=0;p<v.game.n;p++)if(v.game.extra[15]&(1u<<p))ROW("- %s P%u",T("Wolf","狼人"),p+1);
            if(v.game.kind==PG_WOLF && v.game.role==3){ROW("- %s P%u",T("Night victim","今晚被袭击者"),v.game.extra[14]<v.game.n?v.game.extra[14]+1:0);ROW("- %s %u",T("Potions: heal=1 poison=2","药水 解药一 毒药二"),v.game.extra[15]);}
            if(v.game.kind==PG_NIGHT && v.game.role==1 && v.game.extra[0]!=255)ROW("- %s %s",T("Center","中心牌"),role_name(v.game.extra[0],PG_NIGHT));
            if(v.game.kind==PG_NIGHT && v.game.role==3 && v.game.extra[0]!=255)ROW("- %s %s",T("Received role","换得身份"),role_name(v.game.extra[0],PG_NIGHT));
            if((v.game.kind==PG_NIGHT || v.game.kind==PG_WOLF) && v.game.role==2){
                for(unsigned i=0;i<4;i+=2)if(v.game.extra[i]!=255){unsigned p=v.game.extra[i],role=v.game.extra[i+1];
                    if(v.game.kind==PG_WOLF)ROW("- P%u %s",p+1,T(role?"Wolf":"Good",role?"狼人":"好人"));
                    else if(p>=8)ROW("- %s%u %s",T("Center","中心牌"),p-7,role_name(role,PG_NIGHT));
                    else ROW("- P%u %s",p+1,role_name(role,PG_NIGHT));
                }
            }
        }else if(v.game.kind==PG_YACHT){for(unsigned i=0;i<13;i++){if(v.game.extra[i]==255)ROW("- %s --",category(i));else ROW("- %s %u",category(i),v.game.extra[i]);}}
        else {for(unsigned i=0;i<v.game.nh;i++){char card[48];card_name(v.game.kind,v.game.hand[i],card);ROW("- %u %s%s",i+1,card,v.game.kind==PG_LIAR && (liar_mask&(1u<<i))?" [X]":"");}
            if(v.game.kind==PG_CODE && v.game.extra[0]!=255){char card[48];card_name(PG_CODE,v.game.extra[0],card);ROW("- %s %s",T("Held","本轮新牌"),card);}
            if(v.game.kind==PG_LIAR)ROW("- %s %u",T("Claim rank","本轮声明点数"),v.game.extra[14]);
            if(v.game.kind==PG_LOVE && v.game.extra[1])ROW("- P%u = %u",v.game.extra[0]+1,v.game.extra[1]);
        }
    }else if(v.game.kind==PG_CODE){unsigned stride=v.game.n?64/v.game.n:0;for(unsigned p=0;p<v.game.n && count<COUNT(rows);p++){char name[32];seat_name(p,name);ROW("- %s",name);for(unsigned i=0;i<v.game.data[p] && i<stride && count<COUNT(rows);i++){unsigned card=v.game.board[p*stride+i];if(card>=253)ROW("- %u %c ?",i+1,card==254?'W':'B');else{char s[48];card_name(PG_CODE,card,s);ROW("- %u %s",i+1,s);}}}}
    else if(v.game.kind==PG_YACHT){for(unsigned i=0;i<5;i++)ROW("- %s %u: %u %s",T("Die","骰子"),i+1,v.game.board[i],v.game.data[1]&(1u<<i)?"KEEP":"");ROW("- %s %u/3",T("Rolls","已掷次数"),v.game.data[0]);}
    else if(v.game.kind==PG_UNO){char card[48];card_name(PG_UNO,v.game.last,card);ROW("- %s %s",T("Top","桌面牌"),card);ROW("- %s %c",T("Color","当前花色"),'A'+v.game.data[0]);ROW("- %s %u",T("Draw penalty","罚摸牌数"),v.game.data[1]);for(unsigned i=0;i<v.game.n;i++)ROW("- P%u %u %s",i+1,v.game.board[i],T("cards","张"));}
    else if(v.game.kind==PG_AVALON){ROW("- %s %u/%u",T("Success / fail","成功与失败"),v.game.data[1],v.game.data[2]);ROW("- %s %u",T("Team size","队伍人数"),v.game.data[4]);for(unsigned i=0;i<v.game.n;i++)ROW("- P%u %s %s",i+1,v.game.data[0]&(1u<<i)?"TEAM":"",v.game.flags[i]&PG_ACTED?"VOTED":"");for(unsigned i=0;i<5;i++)ROW("- %s %u: %u",T("Mission","任务"),i+1,v.game.board[i]);}
    else if(v.game.kind==PG_LIAR){ROW("- %s P%u x%u",T("Last play","上次出牌"),v.game.last<8?v.game.last+1:0,v.game.data[7]);ROW("- %s %u",T("Claim rank","声明点数"),v.game.data[6]);for(unsigned i=0;i<v.game.n;i++)ROW("- P%u %u %s %u",i+1,v.game.board[i],T("cards / lives","张 生命"),v.game.data[i]);}
    else if(v.game.kind==PG_LOVE){for(unsigned i=0;i<v.game.n;i++){char card[48];card_name(PG_LOVE,v.game.board[i],card);ROW("- P%u %s %s",i+1,T("Discard","弃牌"),card);if(v.game.phase==PG_FINISHED && v.game.board[8+i]){card_name(PG_LOVE,v.game.board[8+i],card);ROW("- P%u %s %s",i+1,T("End hand","剩牌"),card);}}}
    else {ROW("- %s",phase_name(v.game.phase));for(unsigned i=0;i<v.game.n;i++)ROW("- P%u %s%s",i+1,v.game.flags[i]&PG_ALIVE?"IN":"OUT",v.game.flags[i]&PG_ACTED?" VOTED":"");if(v.game.phase==PG_FINISHED)for(unsigned i=0;i<v.game.n;i++)ROW("- P%u %s",i+1,role_name(v.game.board[i],v.game.kind));}
    if(!count)ROW("- %s",T("No cards yet","暂时没有手牌"));
    int n=menu(rows,count);
    if(n>=0 && view_mode==0 && v.game.kind==PG_LIAR && (unsigned)n<v.game.nh){liar_mask^=1u<<n;return;}
    if(n>=0 && view_mode==0 && (v.game.kind==PG_UNO || v.game.kind==PG_LOVE) && (unsigned)n<v.game.nh){initial_a=n;initial_a_set=true;ui_navigate_to(M_PARTY_ACTION);}
#undef ROW
}
typedef struct {unsigned action;const char *en,*zh;} choice_t;
static unsigned choices(choice_t out[8]){
    unsigned n=0;
#define ADD(act,en,zh) do{out[n++]=(choice_t){act,en,zh};}while(0)
    switch(v.game.kind){
    case PG_UNO:ADD(PG_CARD,"Play card","打出手牌");ADD(PG_DRAW,"Draw / penalty","摸牌或接受罚牌");ADD(PG_PASS,"Pass after draw","摸牌后结束回合");ADD(PG_UNO_CALL,"Call UNO","声明只剩一张");ADD(PG_UNO_CATCH,"Catch missed UNO","举报未喊单牌");break;
    case PG_CODE:ADD(PG_GUESS,"Guess a tile","猜对方数字牌");ADD(PG_PASS,"Stop after correct guess","猜对后收手");break;
    case PG_LIAR:ADD(PG_CARD,"Play selected cards","打出勾选的牌");ADD(PG_CHALLENGE,"Call bluff","质疑上一手牌");ADD(PG_PASS,"Accept empty-hand win","认可对方出完牌");break;
    case PG_LOVE:ADD(PG_CARD,"Play card effect","打牌并执行效果");break;
    case PG_YACHT:ADD(PG_ROLL,"Roll unkept dice","重掷未保留骰子");ADD(PG_KEEP,"Toggle keep die","切换保留一颗骰子");ADD(PG_SCORE,"Score category","选择计分项目");break;
    case PG_AVALON:if(v.game.phase==PG_NOMINATE){ADD(PG_SELECT,"Toggle team member","切换任务队员");ADD(PG_ADVANCE,"Submit team","提交任务队伍");}else if(v.game.phase==PG_ASSASSIN){ADD(PG_GUESS,"Guess Merlin","刺杀梅林");}else{ADD(PG_VOTE,"Vote / mission card","投票或任务牌");}break;
    case PG_UNDERCOVER:ADD(PG_VOTE,"Vote a player","投票给一位玩家");ADD(PG_ADVANCE,"Host: open voting","主机开启投票");break;
    case PG_NIGHT:case PG_WOLF:
        if(v.game.phase==PG_DISCUSS){ADD(PG_ADVANCE,"Host: open voting","主机开启投票");}
        else if(v.game.phase==PG_VOTING){ADD(PG_VOTE,"Vote a player","投票给一位玩家");}
        else if(v.game.phase==PG_HUNTER){ADD(PG_GUESS,"Hunter: shoot","猎人开枪");ADD(PG_PASS,"Do not shoot","放弃开枪");}
        else if(v.game.phase==PG_NIGHT_WOLF && v.game.kind==PG_WOLF){ADD(PG_GUESS,"Wolves: choose victim","狼人选择目标");ADD(PG_PASS,"Skip night action","放弃本次行动");}
        else if(v.game.phase==PG_NIGHT_WITCH){ADD(PG_CARD,"Witch: potion","女巫选择药水");ADD(PG_PASS,"Use no potion","不使用药水");}
        else if(v.game.phase==PG_NIGHT_ROBBER || v.game.phase==PG_NIGHT_SWAP){ADD(PG_SWAP,"Swap roles","交换身份");ADD(PG_PASS,"Skip night action","放弃本次行动");}
        else {ADD(PG_PEEK,"Inspect role","查看身份");ADD(PG_PASS,"Skip night action","放弃本次行动");}break;
    case PG_TIC:case PG_REVERSI:ADD(PG_CARD,"Place at cell","在指定位置落子");break;
    default:break;
    }
#undef ADD
    return n;
}
static unsigned a_max(unsigned act){
    unsigned k=v.game.kind,n=v.game.n;
    if(act==PG_UNO_CALL||act==PG_DRAW||act==PG_PASS||act==PG_CHALLENGE||act==PG_ADVANCE||act==PG_ROLL)return 0;
    if(k==PG_TIC)return 8;
    if(k==PG_REVERSI)return 63;
    if((k==PG_UNO&&act==PG_CARD) || k==PG_LOVE)return v.game.nh?v.game.nh-1:0;
    if(k==PG_YACHT)return act==PG_KEEP?4:12;
    if(k==PG_AVALON&&act==PG_VOTE)return 1;
    if(k==PG_WOLF&&act==PG_CARD)return 2;
    if(k==PG_NIGHT&&act==PG_PEEK)return v.game.phase==PG_NIGHT_WOLF?2:n+2;
    return n?n-1:0;
}
static void action(void){
    if(v.watching){view_mode=1;view();return;}
    if(party_game_graphical(v.game.kind)){graphic_game();return;}
    if(v.game.kind==PG_TIC){board();return;}
    if(!v.active || !v.ready || v.game.me>=v.game.n || v.stale){line(11,T("Join / wait for fresh state","请加入并等待同步"));return;}
    choice_t options[8];unsigned count=choices(options);if(!count)return;action_at%=count;unsigned act=options[action_at].action;
    if(param_a>a_max(act))param_a=0;
    unsigned b_limit=v.game.kind==PG_UNO?4:v.game.kind==PG_CODE?(v.game.data[param_a]?v.game.data[param_a]:1):v.game.kind==PG_NIGHT&&act==PG_PEEK?3:v.game.n;
    if(param_b>=b_limit)param_b=0;
    bool use_a=act!=PG_DRAW&&act!=PG_PASS&&act!=PG_CHALLENGE&&act!=PG_ADVANCE&&act!=PG_ROLL&&act!=PG_UNO_CALL;
    bool use_b=(v.game.kind==PG_UNO&&act==PG_CARD)||(v.game.kind==PG_CODE&&act==PG_GUESS)||(v.game.kind==PG_LOVE&&act==PG_CARD)||(v.game.kind==PG_NIGHT&&(act==PG_PEEK||act==PG_SWAP))||(v.game.kind==PG_WOLF&&act==PG_CARD);
    bool use_c=(v.game.kind==PG_CODE&&act==PG_GUESS)||(v.game.kind==PG_LOVE&&act==PG_CARD&&param_a<v.game.nh&&v.game.hand[param_a]==1);
    if(edit_param && btn.pressed){
        unsigned key=btn.id;btn.pressed=false;
        if(key==BTN_ID_SP || key==BTN_ID_BACK)edit_param=0;
        else if(key==BTN_ID_CW || key==BTN_ID_CC){int step=key==BTN_ID_CW?1:-1;unsigned *value=edit_param==1?&param_a:edit_param==2?&param_b:&param_c;unsigned low=edit_param==3&&v.game.kind!=PG_CODE?2:0,limit=edit_param==1?a_max(act)+1:edit_param==2?b_limit:v.game.kind==PG_CODE?12:9;*value=low+(*value-low+limit-low+step)%(limit-low);}
    }
    char a[96],b[96],c[96],title[96];snprintf(title,96,"- %s",ui_language_chinese()?options[action_at].zh:options[action_at].en);
    snprintf(a,96,"- A: %u",param_a+1);snprintf(b,96,"- B: P%u",param_b+1);snprintf(c,96,"- C: %u",param_c);
    unsigned k=v.game.kind;
    if(k==PG_UNO && act==PG_CARD){char card[48];if(param_a<v.game.nh)card_name(k,v.game.hand[param_a],card);else snprintf(card,48,"--");snprintf(a,96,"- %s %u: %s",T("Card","手牌"),param_a+1,card);snprintf(b,96,"- %s %c",T("Wild color","万能牌花色"),'A'+param_b%4);}
    else if(k==PG_LOVE){char card[48];if(param_a<v.game.nh)card_name(k,v.game.hand[param_a],card);else snprintf(card,48,"--");snprintf(a,96,"- %u %s",param_a+1,card);snprintf(b,96,"- %s P%u",T("Target","目标"),param_b+1);snprintf(c,96,"- %s %u",T("Guard guesses 2-8","卫兵猜点数二至八"),param_c);}
    else if(k==PG_CODE){snprintf(a,96,"- %s P%u",T("Target player","目标玩家"),param_a+1);snprintf(b,96,"- %s %u",T("Tile position","牌的位置"),param_b+1);snprintf(c,96,"- %s %u",T("Guess 0-11","猜数字零至十一"),param_c);}
    else if(k==PG_LIAR){snprintf(a,96,"- %s %02X",T("Pick cards (click)","点击勾选手牌"),liar_mask);}
    else if(k==PG_YACHT){if(act==PG_SCORE)snprintf(a,96,"- %s %s",category(param_a),v.game.extra[param_a]==255?"OPEN":"USED");else snprintf(a,96,"- %s %u",T("Die","骰子"),param_a+1);}
    else if(k==PG_AVALON && act==PG_VOTE)snprintf(a,96,"- %s",T(param_a?"YES / SUCCESS":"NO / FAIL",param_a?"赞成或成功":"反对或失败"));
    else if(k==PG_WOLF&&act==PG_CARD)snprintf(a,96,"- %s %u",T("0 skip 1 heal 2 poison","零跳过 一解药 二毒药"),param_a);
    else if(k==PG_NIGHT && act==PG_PEEK){if(v.game.phase==PG_NIGHT_WOLF)snprintf(a,96,"- %s %u",T("Center card","中心牌"),param_a+1);else if(param_a>=v.game.n)snprintf(a,96,"- %s %u",T("Center card","中心牌"),param_a-v.game.n+1);else snprintf(a,96,"- %s P%u",T("Player","玩家"),param_a+1);snprintf(b,96,"- %s %u",T("Second center / player","第二中心牌或玩家"),param_b+1);}
    else if(k==PG_TIC || k==PG_REVERSI){unsigned width=k==PG_TIC?3:8;snprintf(a,96,"- %s %c%u",T("Cell","落子位置"),'A'+param_a%width,param_a/width+1);}
    else {char name[32];seat_name(param_a,name);snprintf(a,96,"- %s",name);}
    if(!use_a)snprintf(a,96,"- --");
    if(!use_b)snprintf(b,96,"- --");
    if(!use_c)snprintf(c,96,"- --");
    if(edit_param){char *label=edit_param==1?a:edit_param==2?b:c;label[0]='>';}
    M_SELECT all[]={{title},{a},{b},{c},{(char *)T(v.pending?"- Waiting for host ACK":"- Confirm action",v.pending?"- 等待主机确认":"- 确认执行操作")},{(char *)T("- Rules and controls","- 规则与操作")}};
    M_SELECT rows[6];unsigned map[6],visible=0;for(unsigned i=0;i<6;i++)if(i==0 || i>=4 || (i==1&&use_a) || (i==2&&use_b) || (i==3&&use_c)){map[visible]=i;rows[visible++]=all[i];}
    int selected=menu(rows,visible);if(selected<0 || v.pending)return;int n=map[selected];
    if(n==0){action_at=(action_at+1)%count;param_a=param_b=0;param_c=k==PG_CODE?0:2;}
    if(n==1 && use_a){if(k==PG_LIAR){view_mode=0;ui_navigate_to(M_PARTY_VIEW);}else edit_param=1;}
    if(n==2 && use_b)edit_param=2;
    if(n==3 && use_c)edit_param=3;
    if(n==4 && request(PARTY_MOVE,act,k==PG_LIAR&&act==PG_CARD?liar_mask:param_a,param_b,param_c)){if(k==PG_LIAR&&act==PG_CARD)liar_mask=0;}
    if(n==5){selected_game=k;ui_navigate_to(M_PARTY_HELP);}
}
typedef struct {const char *en[2],*zh[2];} guide_t;
static const guide_t common[]={
 {{"Lobby: toggle My ready","Host waits for all players"},{"大厅切换我的准备","全部准备后桌主开局"}},
 {{"Invites: choose and join","Watch: public info only"},{"邀请列表选择加入","观战只看公开信息"}},
 {{"Same channel / room scope","Pick table; one per device"},{"同频道与队伍范围","可开多桌 每机一桌"}},
 {{"Host starts after joining","No bots or solo opponents"},{"到齐后主机开始","需要真实设备参加"}},
 {{"UNO: HOLD to show or hide","Shield screen from others"},{"手牌长按显示或隐藏","请遮挡自己的屏幕"}},
 {{"Start opens graphical play","TURN selects a card or die"},{"开局直接图形操作","旋转选择牌或骰子"}},
 {{"TAP plays or keeps a die","BACK returns to room"},{"短按出牌或保留骰子","返回可查看房间"}},
 {{"Wait for host ACK + state","Rejected: choose again"},{"等待主机确认同步","操作被拒请重新选择"}},
 {{"BACK keeps your seat","Leave ends the whole match"},{"返回仍保留座位","确认离开会结束本局"}},
 {{"Reconnect: join same host","Host reboot loses RAM game"},{"重连可恢复原座位","主机重启本局结束"}}
};
static const guide_t rules[TABLETOP_GAMES][6]={
 [PG_UNO]={{{"Four symbols replace colors","Match symbol or number"},{"四种图形代表花色","出同花色或同点数"}},{{"TURN picks; TAP plays","Wild: pick suit then TAP"},{"旋转选牌短按出牌","万能牌再选图形花色"}},{{"+ draws; -> passes","HOLD shows or hides hand"},{"加号摸牌箭头收手","长按显示或隐藏手牌"}},{{"/ skip; <> reverse; +2","Penalties cannot be stacked"},{"斜线跳过双箭头反转","罚摸不能叠加转移"}},{{"UNO: declare one card","!: catch missed UNO"},{"单牌按钮声明剩一张","叹号举报别人未声明"}},{{"Empty hand wins","No +4 challenge rule"},{"先出完手牌者获胜","不含加四质疑规则"}}},
 [PG_UNDERCOVER]={{{"3-8 seats; one undercover","Each sees a similar word"},{"三至八人一名卧底","各自查看相近词语"}},{{"Describe aloud in turns","Never say the secret word"},{"轮流描述自己的词","不能直接说出原词"}},{{"Host opens voting","A selects player; confirm"},{"主机开启投票","选玩家再确认投票"}},{{"No self vote; all alive vote","Most votes: eliminated"},{"活人全投不能投自己","最高票玩家出局"}},{{"Tie: discuss and vote again","Catch undercover: good wins"},{"平票重新讨论投票","找出卧底平民胜"}},{{"Two alive with undercover","Undercover wins"},{"卧底存活到剩两人","则卧底获得胜利"}}},
 [PG_CODE]={{{"B/W tiles 0-11; four each","Own tiles sorted left-right"},{"黑白牌数字零至十一","每人四牌从小到大"}},{{"A opponent; B tile position","C guesses number 0-11"},{"选择对手与牌位置","再设置猜测的数字"}},{{"Correct: reveal opponent tile","Continue guessing or PASS"},{"猜对公开对方的牌","可继续猜或收手"}},{{"Wrong: expose your drawn tile","No deck: expose own tile"},{"猜错公开本轮新牌","无牌则公开自己一牌"}},{{"New tile inserts in order","Positions change every turn"},{"新牌插入后重新排序","注意对方牌位置变化"}},{{"All tiles exposed: out","Last hidden player wins"},{"全部牌公开则出局","最后存活玩家获胜"}}},
 [PG_LIAR]={{{"2-4 players; three lives","Five cards per player"},{"二至四人各三生命","每人每轮五张手牌"}},{{"My cards: click to mark 1-3","Action plays marked cards"},{"手牌页勾选一至三张","操作页打出勾选牌"}},{{"All plays claim table rank","Joker counts as any rank"},{"都声称打出桌面点数","鬼牌可代替任意点"}},{{"Next player plays or calls bluff","Call checks previous play"},{"下家可接牌或质疑","质疑只查上一手牌"}},{{"Liar loses a life; else caller","New deal after each challenge"},{"撒谎者否则质疑者","扣一生命并重新发牌"}},{{"Empty hand needs acceptance","Zero lives out; last alive wins"},{"出完牌需下家认可","生命归零则出局"}}},
 [PG_LOVE]={{{"Play one of two hand cards","A card; B target; C guess"},{"两张手牌选一张打出","选目标与卫兵猜点"}},{{"1 Guard guesses 2-8","2 Priest sees target's card"},{"卫兵猜中淘汰对手","牧师查看对方手牌"}},{{"3 Baron compares ranks","4 Handmaid blocks targeting"},{"男爵比点小者出局","侍女保护到下回合"}},{{"5 Prince forces discard/draw","6 King swaps remaining cards"},{"王子令目标弃牌重摸","国王交换双方手牌"}},{{"7 Countess forced with 5/6","8 Princess discarded: out"},{"伯爵遇王子国王必出","公主被弃则玩家出局"}},{{"Deck ends: highest card wins","Ties: sum of discarded ranks"},{"牌堆空时比剩牌大小","同点再比弃牌点数和"}}},
 [PG_YACHT]={{{"Five dice; up to three rolls","KEEP toggles individual dice"},{"五颗骰子最多掷三次","可逐颗切换保留"}},{{"Choose one unused category","Zero score is allowed"},{"每回合填一个空项目","可以选择得零分"}},{{"Upper: ones through sixes","Upper total 63 adds 35"},{"上半区按点数求和","满六十三加三十五"}},{{"Full house 25; small run 30","Large run 40; five same 50"},{"葫芦小顺二十五三十","大顺五同四十五十"}},{{"Three/four same use total","Chance uses dice total"},{"三同四同达标记总和","全选直接记录总和"}},{{"13 turns per player","Highest total wins"},{"每人完成十三回合","最终总分最高获胜"}}},
 [PG_AVALON]={{{"5-8 players; Merlin/Assassin","Good must pass 3 missions"},{"五至八人梅林和刺客","好人需成功三次任务"}},{{"Leader toggles selected seats","Submit exact team size"},{"队长勾选任务队员","人数符合后提交队伍"}},{{"Everyone votes NO/YES","Majority YES sends team"},{"全员表决反对赞成","过半赞成才执行任务"}},{{"Team: success=1 fail=0","Good cannot play fail"},{"队员投成功或失败","好人只能提交成功"}},{{"Five rejected teams: evil wins","7+ seats mission4 needs 2 fails"},{"连续五次否决坏人胜","七人四任务需两失败"}},{{"3 success: Assassin picks Merlin","Correct assassin: evil wins"},{"三成功后刺客指梅林","刺杀成功仍为坏人胜"}}},
 [PG_NIGHT]={{{"3-8 players plus 3 center cards","One night; roles can change"},{"三至八人三张中心牌","只有一夜身份可交换"}},{{"Wolves see fellow wolves","Lone wolf may peek center"},{"狼人可查看狼同伴","独狼能看一张中心牌"}},{{"Seer: A player or center","Two center: B second center"},{"预言家看一人或中心","看中心时再选第二张"}},{{"Robber swaps self and A","Troublemaker swaps A/B"},{"强盗和目标交换身份","捣蛋鬼交换另外两人"}},{{"Inactive roles wait / discuss","Host opens final vote"},{"无行动身份等待讨论","主机开启最终投票"}},{{"Two votes required to eliminate","Kill a final wolf: good wins"},{"至少两票才会淘汰","杀死最终狼人好人胜"}}},
 [PG_WOLF]={{{"5-8 players; nights and days","Wolves / Seer / Witch"},{"五至八人交替昼夜","狼人预言家与女巫"}},{{"Wolves choose living nonwolf","Seer peeks one player's team"},{"狼人选择活着的好人","预言家查看一人阵营"}},{{"Witch: A=0 skip 1 heal 2 poison","B chooses poison victim"},{"女巫选跳过解药毒药","第二参数选毒药目标"}},{{"Each potion only once per game","Hunter cannot shoot if poisoned"},{"每种药全局只能一次","猎人被毒不能开枪"}},{{"Discuss; host opens vote","Tie means no elimination"},{"讨论后由主机开投票","平票无人出局进夜晚"}},{{"No wolves: good wins","Wolves >= good: wolves win"},{"狼人全灭则好人胜","狼不少于好人则狼胜"}}},
 [PG_TIC]={{{"Two players: X and O","Full 3x3 board on screen"},{"二人分别执叉和圈","三乘三棋盘完整同屏"}},{{"Start opens the board","No scrolling or move form"},{"开局自动进入棋盘","不再滚动或填写位置"}},{{"Rotate: select empty cell","Occupied cells are skipped"},{"旋转选择空格","自动跳过已有棋子"}},{{"Tap once to place your mark","Only on your own turn"},{"轮到自己短按落子","等待对方时不会落子"}},{{"Three in a line wins","Full board: draw"},{"横竖斜三连线获胜","棋盘占满无连线和棋"}},{{"BACK returns to room","View rules or end the match"},{"返回房间可看说明","也可选择结束本局"}}},
 [PG_REVERSI]={{{"Two players on 8x8 board","X first; O second"},{"两人八乘八棋盘","叉先手圈后手"}},{{"Place to bracket opponent disks","Captured lines flip to yours"},{"落子夹住对方棋子","被夹住的棋子翻面"}},{{"Must capture at least one disk","No move: automatic pass"},{"落子必须至少翻一子","无合法位置自动让行"}},{{"Table shows three rows at once","Cursor scrolls all eight rows"},{"桌面每屏显示三行","移动光标滚动全棋盘"}},{{"Both unable to move: finish","Most disks wins; equal draw"},{"双方无法落子时结束","子多者胜相同为和"}},{{"Click cursor then confirm move","No computer opponent"},{"光标短按再确认落子","需要两台真实设备"}}}
};
static unsigned guide_lines(const guide_t *g,char out[4][96]){
    unsigned count=0,width=ui_language_chinese()?100:126;
    for(unsigned row=0;row<2;row++){
        const char *s=ui_language_chinese()?g->zh[row]:g->en[row];
        while(*s && count<4){
            unsigned used=0,last_space=0,pixels=0;
            while(s[used]){unsigned c=(uint8_t)s[used],bytes=c<128?1:c<224?2:3;char glyph[4];if(used+bytes>=96)break;memcpy(glyph,s+used,bytes);glyph[bytes]=0;unsigned advance=ui_zh_width(&u8g2,glyph);if(pixels+advance>width)break;pixels+=advance;memcpy(out[count]+used,s+used,bytes);out[count][used+bytes]=0;if(c==' ')last_space=used;used+=bytes;}
            if(!used)break;
            if(s[used] && last_space && !ui_language_chinese()){used=last_space;out[count][used]=0;}
            s+=used;while(*s==' ')s++;count++;
        }
    }return count;
}
static void help(void){
    char lines[4][96];unsigned logical=COUNT(common)+6;
    u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    if(!help_total){for(unsigned i=0;i<logical;i++){help_offsets[i]=help_total;const guide_t *g=i<COUNT(common)?&common[i]:&rules[selected_game<TABLETOP_GAMES?selected_game:0][i-COUNT(common)];help_total+=(guide_lines(g,lines)+1)/2;}help_offsets[logical]=help_total;}
    unsigned total=help_total;if(!total)return;help_page%=total;
    if(btn.pressed && (btn.id==BTN_ID_CW||btn.id==BTN_ID_CC)){help_page=(help_page+total+(btn.id==BTN_ID_CW?1:-1))%total;btn.pressed=false;}
    if(btn.pressed && btn.id==BTN_ID_SP){back();return;}
    if(help_render_page!=help_page){
        unsigned i=0;while(i+1<logical && help_offsets[i+1]<=help_page)i++;
        const guide_t *g=i<COUNT(common)?&common[i]:&rules[selected_game<TABLETOP_GAMES?selected_game:0][i-COUNT(common)];unsigned n=guide_lines(g,lines),first=(help_page-help_offsets[i])*2;
        for(unsigned row=0;row<2;row++)snprintf(help_render_lines[row],96,"%s",first+row<n?lines[first+row]:"");
        help_render_page=help_page;
    }
    for(unsigned row=0;row<2;row++)ui_zh_draw(&u8g2,1,ui_language_chinese()?11+row*16:9+row*13,help_render_lines[row]);
    char page[24];snprintf(page,sizeof(page),"%u/%u",help_page+1,total);u8g2_DrawStr(&u8g2,104,31,page);
}
void party_pages_proc(void){
    message_service_party_snapshot(&v);u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    bool new_table=v.id!=seen_id || v.host_boot!=seen_boot || memcmp(v.host,seen_host,6);
    if(!v.active || new_table || v.revision!=adapt_revision){adapt_reset();adapt_revision=v.revision;}
    if(!v.active || new_table){graphic_reset();private_open=false;liar_mask=0;board_opened=false;board_revision=~0u;cell=0;seen_id=v.id;seen_boot=v.host_boot;memcpy(seen_host,v.host,6);seen_result=v.result;}
    if(v.game.round!=seen_round){liar_mask=0;seen_round=v.game.round;}
    if(v.result!=seen_result){if(v.result==TEAM_OK)notice(T("Host confirmed","主机已确认"));if(v.result==TEAM_REJECTED)notice(T("Rejected: refresh your choice","操作被拒 请重新选择"));if(v.result==TEAM_TIMEOUT)notice(T("No ACK: check connection","未收到确认 请检查连接"));seen_result=v.result;}
    switch(ui.index){case M_PARTY_HUB:hub();break;case M_PARTY_CREATE:create();break;case M_PARTY_ROOM:room();break;case M_PARTY_VIEW:view();break;case M_PARTY_ACTION:action();break;case M_PARTY_HELP:help();break;case M_PARTY_LEAVE:{M_SELECT rows[]={{(char *)T("- Cancel","- 取消")},{(char *)T("- End match for everyone","- 确认结束整局游戏")}};int n=menu(rows,2);if(n==0)back();if(n==1 && request(PARTY_LEAVE,0,0,0,0))back();break;}default:break;}
    if(esp_timer_get_time()<error_until && !(party_game_graphical(v.game.kind) && (ui.index==M_PARTY_VIEW || ui.index==M_PARTY_ACTION))){u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,0,15,128,17);u8g2_SetDrawColor(&u8g2,1);line(27,error_text);}
}
