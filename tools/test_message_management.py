"""Exercise production message filtering and reply guards without radio or NVS."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'firmware/ui/team_pages.inc').read_text(encoding='utf8')
def extract(name):
 start=source.index('static '+name);brace=source.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[start:end]+'\n'
shim=r'''
#include "message_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
typedef struct{const char *m_select;}M_SELECT;
enum{M_MESSAGE_DELETE=87,M_REPLY_CONFIRM=86,M_TX_STATUS=30};
static message_inbox_entry_t s_message;
static unsigned s_message_filter;
static uint8_t s_filter_peer[6],s_reply_peer[6],s_reply_channel,channel=1;
static bool s_reply,friend=true,save_ok=true;
static char s_draft[129],s_send_text[129],saved[129],last_notice[64];
static uint32_t s_tx_id;
static unsigned navigated,sends,edits;
static int selected=-1;
typedef struct{bool selecting_group;}draft_selector_t;
static draft_selector_t s_draft_selector;
static int menu(M_SELECT *rows,unsigned n){assert(rows&&n);int r=selected;selected=-1;return r;}
static void back(void){}
static void notice(const char *s){snprintf(last_notice,sizeof(last_notice),"%s",s);}
static void load_draft(void){}
static const char *ui_zh_lookup(const char *s){return s;}
static void open_draft_editor(void){edits++;}
static void ui_navigate_to(unsigned page){navigated=page;}
static bool friend_mgr_is_friend(const uint8_t *mac){(void)mac;return friend;}
static bool draft_store_save_work(const char *s){if(!save_ok)return false;strcpy(saved,s);return true;}
uint8_t message_service_get_channel(void){return channel;}
bool message_service_send_text(message_kind_t kind,const uint8_t *mac,uint8_t ch,const char *s,uint16_t n,bool ack,uint32_t *id){
 assert(kind==MESSAGE_KIND_QUICK&&n==strlen(s)&&n&&id);assert(ch?mesh_packet_is_broadcast(mac)&&!ack:ack&&mesh_mac_equal(mac,s_reply_peer));sends++;return true;}
'''
tests=r'''
int main(void){
 s_message=(message_inbox_entry_t){.channel=1,.unread=true,.len=5};strcpy(s_message.text,"hello");s_message.src_mac[0]=2;s_message.src_mac[5]=4;
 for(unsigned i=0;i<3;i++){s_message_filter=i;assert(message_matches(&s_message));}s_message.unread=false;s_message_filter=1;assert(!message_matches(&s_message));
 s_message_filter=3;assert(!message_matches(&s_message));memcpy(s_filter_peer,s_message.src_mac,6);assert(message_matches(&s_message));
 s_message.channel=0;s_message.len=9;strcpy(s_message.text,"[ALL] Hi!");s_message_filter=4;assert(message_matches(&s_message));
 /* Quote save failure and overflow preserve the working draft. */
 strcpy(s_draft,"reply");save_ok=false;selected=2;message_actions_page();assert(!strcmp(s_draft,"reply")&&!s_reply&&!edits);
 save_ok=true;memset(s_draft,'X',128);s_draft[128]=0;selected=2;message_actions_page();assert(strlen(s_draft)==128&&!s_reply);
 strcpy(s_draft,"reply");selected=2;message_actions_page();assert(s_reply&&!s_reply_channel&&edits==1&&strstr(s_draft,"[ALL] Hi!")&&strstr(s_draft,"reply"));
 strcpy(s_send_text,s_draft);selected=0;reply_confirm_page();assert(!sends);selected=2;reply_confirm_page();assert(sends==1&&!s_reply&&navigated==30);
 friend=false;selected=1;message_actions_page();assert(!s_reply);s_message.channel=2;selected=1;message_actions_page();assert(s_reply&&s_reply_channel==2);
 selected=2;reply_confirm_page();assert(sends==1);channel=2;selected=2;reply_confirm_page();assert(sends==2&&!s_reply);
 puts("PASS message management: filters, quote atomicity/overflow, private/public routing, channel and cancel guards");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(shim+''.join(extract(n) for n in ['bool message_matches','void message_actions_page','void reply_confirm_page'])+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),str(root/'build/mesh-packet-host.o'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
