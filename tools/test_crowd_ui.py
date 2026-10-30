"""Production crowd menus with fake service: confirmations, bounds and non-destructive guards."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'firmware/ui/crowd_pages.inc').read_text(encoding='utf8')
shim=r'''
#include "message_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define FRIEND_NAME_MAX 13
enum{M_CROWD_CREATE=90,M_CROWD_CONFIRM,M_ACTIVITY_HISTORY=93,M_ACTIVITY_DETAIL,M_NETWORK_STATUS=68};
typedef struct{const char *m_select;}M_SELECT;
static unsigned s_crowd_kind,s_crowd_action,s_activity_index,actions,navigated,edits,last_count,last_status;
static char s_crowd_title[33],s_form_text[129],last_notice[96];
static activity_record_t s_activity_detail;
static message_crowd_view_t fixture;static int selected=-1;static bool action_ok=true;
static const char *ui_zh_lookup(const char *s){return s;}
static int64_t esp_timer_get_time(void){return 10000000;}
static void footer(const char *s){assert(s);}
static void text_line(int y,const char *s){assert(y>=0&&y<=31&&s);}
static void message_sender_name(const uint8_t *mac,char *name){snprintf(name,14,"Peer%02X",mac[5]);}
static int menu(M_SELECT *rows,unsigned count){assert(count&&count<=16);last_count=count;for(unsigned i=0;i<count;i++)assert(rows[i].m_select&&strlen(rows[i].m_select)<128);int n=selected;selected=-1;return n;}
static void ui_navigate_to(unsigned page){navigated=page;}
static void open_content_editor(char *target,size_t cap){assert(target==s_form_text&&cap==33);edits++;}
static void notice(const char *s){snprintf(last_notice,sizeof(last_notice),"%s",s);}
static void back(void){}
void message_service_crowd_view(message_crowd_view_t *v){*v=fixture;}
bool message_service_crowd_action(unsigned action,unsigned kind,const char *title,unsigned status){assert(kind<2);if(action==MESSAGE_CROWD_CREATE)assert(title&&strlen(title)<=32);actions++;last_status=status;return action_ok;}
void message_service_network_view(message_network_view_t *out){memset(out,0,sizeof(*out));out->forwarded=out->duplicates=out->dropped=0xffffffff;}
void message_service_snapshot(message_service_snapshot_t *out){memset(out,255,sizeof(*out));}
uint8_t message_service_get_channel(void){return 16;}
void message_service_diagnostics(message_diagnostics_t *out){memset(out,255,sizeof(*out));out->records=12;}
bool message_service_activity_get(unsigned i,activity_record_t *out){if(i>=12)return false;*out=(activity_record_t){.kind=(i%4)+1,.channel=16,.sequence=i,.values={8,8,8,8},.title="Record"};return true;}
'''
tests=r'''
int main(void){
 crowd_page(0);assert(last_count==1&&!actions);selected=0;crowd_page(0);assert(navigated==90&&!actions);
 strcpy(s_form_text,"Name");selected=2;crowd_create_page();assert(navigated==91&&!actions&&!strcmp(s_crowd_title,"Name"));
 strcpy(s_form_text,"Changed after preview");selected=0;crowd_confirm_page();assert(!actions);selected=2;crowd_confirm_page();assert(actions==1&&!strcmp(s_crowd_title,"Name"));
 memset(s_form_text,'X',33);s_form_text[33]=0;selected=2;crowd_create_page();assert(actions==1&&strstr(last_notice,"1-32"));
 fixture.rooms[0]=(crowd_room_t){.active=true,.local=true,.open=true,.count=8,.title="Check"};for(unsigned i=0;i<8;i++){fixture.rooms[0].people[i].mac[5]=i;fixture.rooms[0].people[i].status=(i%3)+1;}
 crowd_page(0);assert(last_count==15);selected=3;crowd_page(0);assert(actions==2&&last_status==2);
 fixture.rooms[1]=(crowd_room_t){.active=true,.open=true,.title="Quiz"};selected=2;crowd_page(1);assert(actions==3&&last_status==1);
 network_diagnostics_page();assert(last_count==11);selected=0;activity_history_page();assert(last_count==12);activity_detail_page();assert(last_count==6);
 puts("PASS crowd UI: menu bounds, immutable create preview, explicit confirm/cancel, length guard, status routing, diagnostics/history bounds");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),str(root/'build/mesh-packet-host.o'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
