"""Exercise actual party page handlers with fake UI/service, never RF claims."""
from pathlib import Path
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[1]
graphics=(root/'firmware/ui/party_graphics.inc').read_text(encoding='utf8')
adapt=(root/'firmware/ui/party_adapt.inc').read_text(encoding='utf8')
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/ui/party_pages.c').read_text(encoding='utf8').replace('#include "party_graphics.inc"',graphics).replace('#include "party_adapt.inc"',adapt),flags=re.M)
shim=r'''
#include "party_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
enum{M_PARTY_HUB=109,M_PARTY_ROOM,M_PARTY_VIEW,M_PARTY_ACTION,M_PARTY_HELP,M_PARTY_CREATE,M_PARTY_LEAVE};
enum{BTN_ID_CW=1,BTN_ID_CC,BTN_ID_SP,BTN_ID_BACK,BTN_ID_LP,S_LAYER_OUT};
static struct{unsigned index,layer,state,num[120],select[20],parent_index[20];bool init;}ui;
static struct{float y,y_trg,box_y,box_y_trg[20];bool loop;}list;
static struct{bool pressed;unsigned id;}btn;
typedef struct{char *m_select;}M_SELECT;
static int u8g2,u8g2_font_5x7_tf;
static bool chinese=true;static party_snapshot_t fixture;
static uint8_t joined_host[6];static uint32_t joined_boot,joined_id;
static unsigned navigations,sends,creates,rendered,last_action,last_a,last_b,last_c,width_calls;
static unsigned grid_lines,cross_lines,circles;
#define U8G2_DRAW_ALL 15
static int64_t clock_us;
static bool ui_language_chinese(void){return chinese;}
static int64_t esp_timer_get_time(void){return clock_us;}
static void ui_navigate_to(unsigned page){ui.parent_index[++ui.layer]=ui.index;ui.index=page;navigations++;}
static void list_update_scroll_targets(void){list.y_trg=-16.0f*ui.select[ui.layer];}
static void list_show(M_SELECT *rows,unsigned page){assert(page>=109&&page<=115&&ui.num[page]&&ui.num[page]<=40);assert(ui.select[ui.layer]<ui.num[page]);rendered=ui.num[page];for(unsigned i=0;i<ui.num[page];i++)assert(rows[i].m_select&&strlen(rows[i].m_select)<128);}
static void list_rotate_switch(void){unsigned n=ui.num[ui.index];ui.select[ui.layer]=(ui.select[ui.layer]+(btn.id==BTN_ID_CW?1:n-1))%n;}
static void u8g2_SetDrawColor(int *d,int c){(void)d;assert(c==0||c==1);}
static void u8g2_SetFont(int *d,int f){(void)d;(void)f;}
static void u8g2_DrawBox(int *d,int x,int y,int w,int h){(void)d;assert(x>=0&&y>=0&&x+w<=128&&y+h<=32);}
static void u8g2_DrawHLine(int *d,int x,int y,int w){u8g2_DrawBox(d,x,y,w,1);grid_lines++;}
static void u8g2_DrawVLine(int *d,int x,int y,int h){u8g2_DrawBox(d,x,y,1,h);grid_lines++;}
static void u8g2_DrawLine(int *d,int x,int y,int a,int b){(void)d;assert(x>=0&&y>=0&&a>=0&&b>=0&&x<31&&a<31&&y<31&&b<31);cross_lines++;}
static void u8g2_DrawCircle(int *d,int x,int y,int r,int mask){(void)d;assert(x-r>=0&&y-r>=0&&x+r<31&&y+r<31&&mask==15);circles++;}
static void u8g2_DrawStr(int *d,int x,int y,const char *s){(void)d;assert(x>=0&&x<128&&y>=0&&y<32&&s);}
static void ui_zh_draw_fit(int *d,int x,int y,const char *s,unsigned width){u8g2_DrawStr(d,x,y,s);assert(width<=128);}
static void ui_zh_draw(int *d,int x,int y,const char *s){u8g2_DrawStr(d,x,y,s);}
static unsigned ui_zh_width(int *d,const char *s){(void)d;width_calls++;unsigned n=0;while(*s){unsigned c=(unsigned char)*s;if(c<128){n+=5;s++;}else{n+=12;s+=3;}}return n;}
static void message_service_party_snapshot(party_snapshot_t *v){*v=fixture;}
static bool message_service_network_enable(bool on){fixture.enabled=on;return true;}
static bool message_service_party_watch_table(const uint8_t *host,uint32_t boot,uint32_t id){assert(host&&boot&&id);sends++;return true;}
static bool message_service_party_stop_watching(void){fixture.watching=false;return true;}
static bool message_service_party_join_table(const uint8_t *host,uint32_t boot,uint32_t id){assert(host&&boot&&id);memcpy(joined_host,host,6);joined_boot=boot;joined_id=id;sends++;return true;}
static bool message_service_party_create(unsigned kind){assert(kind<TABLETOP_GAMES);creates++;return true;}
static bool message_service_party_request(unsigned op,unsigned action,unsigned a,unsigned b,unsigned c){assert(op>=1&&op<=7);sends++;last_action=action;last_a=a;last_b=b;last_c=c;return true;}
'''
tests=r'''
static void enter(unsigned page){ui.index=page;party_pages_enter(page);btn.pressed=false;party_pages_proc();}
static void key(unsigned id){btn.pressed=true;btn.id=id;party_pages_proc();}
int main(void){
 assert(!strcmp(role_name(1,PG_UNDERCOVER),"卧底"));
 for(unsigned lang=0;lang<2;lang++){
  chinese=lang;
  for(unsigned k=0;k<TABLETOP_GAMES;k++)if(party_game_available(k))for(unsigned j=0;j<COUNT(common)+6;j++){
   const guide_t *g=j<COUNT(common)?&common[j]:&rules[k][j-COUNT(common)];char lines[4][96];unsigned n=guide_lines(g,lines);assert(n>=2&&n<=4);for(unsigned i=0;i<n;i++)assert(ui_zh_width(&u8g2,lines[i])<=(lang?100u:126u));
  }
  fixture=(party_snapshot_t){.active=true,.local=true,.enabled=true,.ready=true,.id=1+lang,.host_boot=1,.game={.kind=PG_UNO,.n=2,.me=0,.nh=7,.phase=PG_PLAYING,.direction=1}};
  enter(M_PARTY_HUB);assert(rendered==14);
  for(unsigned kind=0;kind<TABLETOP_GAMES;kind++)if(party_game_available(kind)){
   fixture.game.kind=kind;fixture.id++;fixture.game.phase=PG_PLAYING;fixture.game.me=0;
   table_browser=false;selected_game=kind;enter(M_PARTY_CREATE);assert(rendered==4);enter(M_PARTY_ROOM);assert(ui.index==(party_game_graphical(kind)?M_PARTY_VIEW:M_PARTY_ROOM));
   selected_game=kind;enter(M_PARTY_HELP);unsigned calls=width_calls;for(unsigned i=0;i<50;i++)party_pages_proc();assert(width_calls==calls);for(unsigned i=0;i<28;i++)key(BTN_ID_CW);
  }
 }
 fixture.game=(party_view_t){.kind=PG_UNO,.phase=PG_PLAYING,.n=2,.me=0,.turn=0,.nh=7,.last=3};fixture.id++;view_mode=1;enter(M_PARTY_VIEW);
 unsigned prior=sends;key(BTN_ID_SP);assert(sends==prior&&!graphic_reveal);key(BTN_ID_LP);assert(graphic_reveal);key(BTN_ID_SP);assert(sends==prior+1&&last_action==PG_CARD&&last_a==0);
 graphic_pick=6;key(BTN_ID_CW);key(BTN_ID_SP);assert(last_action==PG_DRAW);key(BTN_ID_CW);key(BTN_ID_SP);assert(last_action==PG_PASS);key(BTN_ID_CW);key(BTN_ID_SP);assert(last_action==PG_UNO_CALL);
 graphic_pick=0;fixture.game.hand[0]=52;key(BTN_ID_SP);assert(graphic_wild);prior=sends;key(BTN_ID_CW);key(BTN_ID_SP);assert(sends==prior+1&&last_b==1&&!graphic_wild);
 fixture.game.turn=1;prior=sends;key(BTN_ID_SP);assert(sends==prior);fixture.game.turn=0;fixture.pending=true;key(BTN_ID_SP);assert(sends==prior);fixture.pending=false;
 enter(M_PARTY_VIEW);assert(!graphic_reveal);key(BTN_ID_LP);fixture.game.phase=PG_FINISHED;key(BTN_ID_SP);assert(sends==prior);
 fixture.game=(party_view_t){.kind=PG_YACHT,.phase=PG_PLAYING,.n=2,.me=0,.turn=0};memset(fixture.game.extra,255,16);fixture.id++;enter(M_PARTY_VIEW);graphic_pick=5;key(BTN_ID_SP);assert(last_action==PG_ROLL);graphic_pick=2;key(BTN_ID_SP);assert(last_action==PG_KEEP&&last_a==2);
 graphic_pick=6;key(BTN_ID_SP);assert(graphic_scores);key(BTN_ID_CW);key(BTN_ID_SP);assert(last_action==PG_SCORE&&last_a==1&&!graphic_scores);graphic_pick=6;key(BTN_ID_SP);fixture.game.extra[0]=0;prior=sends;key(BTN_ID_SP);assert(sends==prior);key(BTN_ID_BACK);assert(!graphic_scores&&!btn.pressed);
 fixture.game=(party_view_t){.kind=PG_TIC,.phase=PG_PLAYING,.n=2,.me=0,.turn=0};fixture.game.board[0]=1;fixture.game.board[4]=2;fixture.id++;view_mode=1;enter(M_PARTY_VIEW);cell=0;board_revision=~0u;grid_lines=cross_lines=circles=0;party_pages_proc();assert(cell==1&&grid_lines==8&&cross_lines==2&&circles==1);
 prior=sends;fixture.game.turn=1;key(BTN_ID_SP);assert(sends==prior);fixture.game.turn=0;fixture.pending=true;key(BTN_ID_SP);assert(sends==prior);fixture.pending=false;key(BTN_ID_SP);assert(sends==prior+1&&last_a==1);
 fixture.game.phase=PG_FINISHED;key(BTN_ID_SP);assert(sends==prior+1);


 fixture.game=(party_view_t){.kind=PG_CODE,.phase=PG_PLAYING,.n=3,.me=0,.turn=0,.nh=4};fixture.game.data[0]=4;fixture.game.data[1]=7;fixture.game.data[2]=5;memset(fixture.game.board,253,64);fixture.id++;view_mode=1;enter(M_PARTY_VIEW);for(unsigned i=0;i<5;i++)key(BTN_ID_CW);prior=sends;key(BTN_ID_SP);assert(ui.index==M_PARTY_VIEW&&adapt_stage==2&&adapt_card==1&&adapt_target==5);key(BTN_ID_CW);key(BTN_ID_SP);assert(sends==prior&&adapt_stage==3);key(BTN_ID_SP);assert(sends==prior+1&&last_action==PG_GUESS&&last_a==1&&last_b==5&&last_c==1&&!adapt_stage);
 key(BTN_ID_SP);key(BTN_ID_BACK);assert(!adapt_stage&&!btn.pressed);key(BTN_ID_SP);fixture.revision++;party_pages_proc();assert(!adapt_stage);key(BTN_ID_SP);fixture.pending=true;prior=sends;key(BTN_ID_SP);assert(sends==prior&&!adapt_stage);fixture.pending=false;
 cell=12;key(BTN_ID_SP);assert(last_action==PG_PASS);
 fixture.game=(party_view_t){.kind=PG_LIAR,.phase=PG_PLAYING,.n=3,.me=0,.turn=0,.nh=5};fixture.id++;view_mode=0;enter(M_PARTY_VIEW);prior=sends;key(BTN_ID_SP);assert(!private_open&&sends==prior);key(BTN_ID_LP);key(BTN_ID_SP);assert(liar_mask==1);key(BTN_ID_CW);key(BTN_ID_CW);key(BTN_ID_SP);assert(liar_mask==5);for(unsigned i=0;i<3;i++)key(BTN_ID_CW);key(BTN_ID_SP);assert(ui.index==M_PARTY_VIEW&&adapt_stage==3&&sends==prior);key(BTN_ID_SP);assert(last_action==PG_CARD&&last_a==5&&!liar_mask);
 cell=6;key(BTN_ID_SP);key(BTN_ID_SP);assert(last_action==PG_CHALLENGE);cell=7;key(BTN_ID_SP);key(BTN_ID_SP);assert(last_action==PG_PASS);
 cell=0;liar_mask=7;cell=3;key(BTN_ID_SP);assert(liar_mask==7);fixture.game.nh=0;cell=0;prior=sends;key(BTN_ID_SP);assert(sends==prior);adapt_reset();liar_mask=0;
 fixture.game=(party_view_t){.kind=PG_LOVE,.phase=PG_PLAYING,.n=2,.me=0,.turn=0,.nh=2};memset(fixture.game.flags,PG_ALIVE,2);fixture.game.hand[0]=1;fixture.game.hand[1]=5;fixture.id++;view_mode=0;enter(M_PARTY_VIEW);key(BTN_ID_LP);key(BTN_ID_CW);prior=sends;key(BTN_ID_SP);assert(ui.index==M_PARTY_VIEW&&adapt_stage==1);key(BTN_ID_CW);key(BTN_ID_SP);assert(adapt_stage==3&&adapt_target==1);key(BTN_ID_SP);assert(sends==prior+1&&last_action==PG_CARD&&last_a==1&&last_b==1);
 cell=0;key(BTN_ID_SP);assert(adapt_stage==1&&adapt_target==1);key(BTN_ID_SP);assert(adapt_stage==2);key(BTN_ID_CC);assert(adapt_guess==8);key(BTN_ID_SP);key(BTN_ID_SP);assert(last_a==0&&last_b==1&&last_c==8);
 fixture.game.hand[0]=4;cell=0;key(BTN_ID_SP);assert(adapt_stage==3);prior=sends;key(BTN_ID_CW);key(BTN_ID_SP);assert(!adapt_stage&&sends==prior);
 fixture.game.hand[0]=5;fixture.game.hand[1]=7;key(BTN_ID_SP);assert(!adapt_stage);fixture.game.hand[0]=1;fixture.game.hand[1]=2;fixture.game.flags[1]|=PG_PROTECTED;key(BTN_ID_SP);assert(adapt_stage==2);adapt_reset();
 fixture.game.flags[1]=PG_ALIVE;fixture.game.turn=1;prior=sends;key(BTN_ID_SP);assert(sends==prior&&!adapt_stage);fixture.game.turn=0;key(BTN_ID_SP);enter(M_PARTY_VIEW);assert(!adapt_stage&&!private_open);
 fixture.game=(party_view_t){.kind=PG_UNDERCOVER,.phase=PG_DISCUSS,.n=3,.me=0,.turn=0};memset(fixture.game.flags,PG_ALIVE,3);fixture.id++;view_mode=1;enter(M_PARTY_VIEW);key(BTN_ID_SP);assert(last_action==PG_ADVANCE);fixture.game.phase=PG_VOTING;key(BTN_ID_CW);key(BTN_ID_SP);assert(last_action==PG_VOTE&&last_a==1);prior=sends;fixture.pending=true;key(BTN_ID_SP);assert(sends==prior);fixture.pending=false;
 for(unsigned k=0;k<TABLETOP_GAMES;k++)if(party_game_available(k)&&!party_game_graphical(k)){fixture.game.kind=k;fixture.game.phase=PG_PLAYING;fixture.game.n=party_min(k);fixture.game.me=0;fixture.game.nh=2;fixture.id++;view_mode=0;enter(M_PARTY_VIEW);assert(!private_open);key(BTN_ID_SP);assert(!private_open);}

 ui.layer=0;fixture=(party_snapshot_t){.enabled=true};fixture.game.me=255;
 fixture.tables[0]=(party_table_t){.active=true,.host={2,0,0,0,0,1},.kind=PG_UNO,.phase=PG_LOBBY,.n=2,.id=1,.host_boot=100};strcpy(fixture.tables[0].name,"Alice");
 fixture.tables[1]=(party_table_t){.active=true,.host={2,0,0,0,0,2},.kind=PG_LOVE,.phase=PG_LOBBY,.n=1,.id=1,.host_boot=100};strcpy(fixture.tables[1].name,"Bob");
 enter(M_PARTY_HUB);ui.select[ui.layer]=12;key(BTN_ID_SP);assert(ui.index==M_PARTY_CREATE&&table_browser&&table_count==2);party_pages_enter(M_PARTY_CREATE);btn.pressed=false;party_pages_proc();assert(rendered==4);
 prior=sends;ui.select[ui.layer]=3;key(BTN_ID_SP);assert(table_detail&&sends==prior&&table_choice.host[5]==2);btn.pressed=false;party_pages_proc();assert(rendered==7);ui.select[ui.layer]=4;key(BTN_ID_SP);assert(ui.index==M_PARTY_ROOM&&sends==prior+1&&joined_host[5]==2&&joined_boot==100&&joined_id==1);
 ui.layer=0;table_browser=false;selected_game=PG_UNO;enter(M_PARTY_CREATE);ui.select[ui.layer]=3;key(BTN_ID_SP);assert(table_browser&&table_count==1&&table_rows[0].kind==PG_UNO);
 ui.select[ui.layer]=2;key(BTN_ID_SP);assert(table_detail);fixture.tables[0].active=false;prior=sends;ui.select[ui.layer]=4;key(BTN_ID_SP);assert(sends==prior);key(BTN_ID_BACK);assert(!table_detail);ui.select[ui.layer]=0;key(BTN_ID_SP);assert(!table_count);key(BTN_ID_BACK);assert(!table_browser&&ui.index==M_PARTY_CREATE&&!btn.pressed);
 fixture.active=true;fixture.ready=true;fixture.id=1;fixture.host_boot=100;fixture.game=(party_view_t){.kind=PG_LOVE,.phase=PG_PLAYING,.n=2,.me=0,.nh=2};fixture.host[5]=1;view_mode=0;enter(M_PARTY_VIEW);key(BTN_ID_LP);assert(private_open);fixture.host[5]=2;party_pages_proc();assert(!private_open&&!adapt_stage);

 ui.layer=0;fixture=(party_snapshot_t){.active=true,.local=true,.enabled=true,.ready=true,.id=9,.host_boot=1,.ready_mask=1,.game={.kind=PG_UNO,.n=2,.me=0,.phase=PG_LOBBY}};enter(M_PARTY_ROOM);prior=sends;ui.select[ui.layer]=2;key(BTN_ID_SP);assert(sends==prior);ui.select[ui.layer]=8;key(BTN_ID_SP);assert(last_action==0&&last_a==0);ui.select[ui.layer]=9;key(BTN_ID_SP);assert(sends==prior+2);
 fixture.ready_mask=3;ui.select[ui.layer]=2;key(BTN_ID_SP);assert(sends==prior+3);
 fixture.tables[0]=(party_table_t){.active=true,.invited=true,.kind=PG_UNO,.phase=PG_LOBBY,.id=9,.host_boot=1};fixture.tables[1]=(party_table_t){.active=true,.kind=PG_LOVE,.phase=PG_LOBBY,.id=2,.host_boot=2};enter(M_PARTY_HUB);ui.select[ui.layer]=13;key(BTN_ID_SP);assert(table_browser&&table_count==1&&table_filter==TABLETOP_GAMES+1);
 ui.layer=0;fixture.watching=true;fixture.local=false;fixture.game.me=255;fixture.game.phase=PG_PLAYING;fixture.game.nh=0;view_mode=0;enter(M_PARTY_VIEW);assert(view_mode==1&&!private_open);prior=sends;key(BTN_ID_SP);key(BTN_ID_LP);key(BTN_ID_SP);assert(sends==prior);
 fixture.game.kind=PG_LOVE;view_mode=0;enter(M_PARTY_VIEW);key(BTN_ID_LP);assert(view_mode==1&&!private_open&&sends==prior);
 for(unsigned k=0;k<TABLETOP_GAMES;k++)if(party_game_available(k)){fixture.game.kind=k;fixture.game.phase=PG_PLAYING;view_mode=1;enter(M_PARTY_VIEW);key(BTN_ID_SP);assert(sends==prior);}
 fixture.game.kind=PG_LOVE;enter(M_PARTY_ROOM);ui.select[ui.layer]=7;key(BTN_ID_SP);assert(!fixture.watching&&sends==prior);
 puts("PASS lobby UI: all-ready start guard, own-ready/invite actions, invitation filter, ten-game readonly spectators, private reveal blocked, local spectator exit");
 puts("PASS table browser: all/filter/refresh/back, selected host identity, no automatic JOIN, cross-host private view reset");
 puts("PASS graphical UI: card privacy/wild suit/actions, five dice/keep/roll/score, full tic board; wrong turn/pending/finished never submits");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-party-ui-') as temp:
    p=Path(temp);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
    subprocess.run(['gcc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Ifirmware/app','-Ifirmware/protocol',str(p/'test.c'),'firmware/app/party_rules.c','-o',str(p/'test.exe')],cwd=root,check=True)
    subprocess.run([str(p/'test.exe')],check=True,timeout=20)
