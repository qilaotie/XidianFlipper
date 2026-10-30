"""Production network action UI with fake transport; no real messages transmitted."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
local=(root/'firmware/ui/local_pages.c').read_text(encoding='utf8')
def extract(signature):
 start=local.index(signature);brace=local.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(local[end]=='{')-(local[end]=='}');end+=1
 return local[start:end]+'\n'
shim=r'''
#include "message_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define FRIEND_STORE_MAX 16
enum{NET_GROUP,NET_ANNOUNCEMENT,NET_OFFLINE,NET_CUSTODY};
enum{M_MESSAGE_COMPOSE=47,M_NETWORK_STATUS=68,M_NET_SEND,M_OFFLINE_RECIPIENT,M_NET_CONFIRM,M_OUTBOX,M_OUTBOX_ITEM,M_OUTBOX_DELETE,M_TX_STATUS=30};
typedef struct{const char *m_select;}M_SELECT;
typedef struct{char name[14];uint8_t mac[6];}friend_record_t;
static struct{unsigned index;}ui;
static unsigned s_net_mode,s_outbox_slot;
static uint8_t s_offline_dst[6];
static uint32_t s_tx_id;
static char s_draft[129],s_send_text[129],last_notice[64];
static int selected=-1;static unsigned friend_count=2,sends,queues,deletes,retries,backs;
static bool save_ok=true,network_on=true,action_ok=true,storage_error;
static outbox_entry_t entries[OUTBOX_SLOTS];
static void load_draft(void){}
int message_service_custody_text(const uint8_t dst[6],const char *text){return message_service_offline_text(dst,text);}
static const char *ui_zh_lookup(const char *s){return !strcmp(s,"OUTBOX WAIT")?"WAIT":s;}
static void open_content_editor(char *target,size_t capacity){assert(target==s_draft&&capacity==129);ui.index=100;}
static bool draft_store_save_work(const char *text){(void)text;return save_ok;}
static int menu(M_SELECT *rows,unsigned count){assert(rows&&count);for(unsigned i=0;i<count;i++)assert(rows[i].m_select&&strlen(rows[i].m_select)<40);int r=selected;selected=-1;return r;}
static void ui_navigate_to(unsigned page){ui.index=page;}
static void notice(const char *s){snprintf(last_notice,sizeof(last_notice),"%s",s);}
static void back(void){backs++;}
static void text_line(int y,const char *s){assert(y>=0&&y<=31&&s);}
static void footer(const char *s){assert(s);}
uint8_t message_service_get_channel(void){return 16;}
static unsigned friend_mgr_friend_count(void){return friend_count;}
static bool friend_mgr_get_friend(unsigned i,friend_record_t *p){if(i>=friend_count)return false;memset(p,0,sizeof(*p));strcpy(p->name,"Test peer");p->mac[0]=2;p->mac[5]=(uint8_t)(i+2);return true;}
bool message_service_group_text(const char *text,bool announcement,uint32_t *id){assert(!strcmp(text,s_send_text));assert(announcement==(s_net_mode==NET_ANNOUNCEMENT));if(!network_on)return false;sends++;*id=1;return true;}
int message_service_offline_text(const uint8_t *dst,const char *text){assert(dst[5]==2&&!strcmp(text,s_send_text));if(!action_ok)return -1;queues++;return 0;}
bool message_service_outbox_get(unsigned slot,outbox_entry_t *e,bool *error){assert(slot<6);*e=entries[slot];*error=storage_error;return true;}
bool message_service_outbox_retry(unsigned slot){assert(slot<6);retries++;return action_ok;}
bool message_service_outbox_remove(unsigned slot){assert(slot<6);if(!action_ok)return false;deletes++;return true;}
void message_service_network_view(message_network_view_t *v){memset(v,0,sizeof(*v));v->enabled=network_on;}
'''
production=''.join(extract(s) for s in ['static void net_send_page(','static void offline_recipient_page(','static void net_confirm_page(','static const char *outbox_state(','static void outbox_page(','static void outbox_item_page(','static void outbox_delete_page('])
tests=r'''
int main(void){
 for(unsigned mode=0;mode<3;mode++){
  s_net_mode=mode;strcpy(s_draft,"Hi (^_^)");ui.index=69;selected=1;net_send_page();assert(ui.index==(mode==NET_OFFLINE?70:71));
  if(mode==NET_OFFLINE){selected=0;offline_recipient_page();assert(ui.index==71&&s_offline_dst[5]==2);}
  unsigned before=sends+queues;selected=1;net_confirm_page();assert(sends+queues==before);
  selected=0;net_confirm_page();assert(sends+queues==before);
  selected=2;net_confirm_page();assert(sends+queues==before+1&&ui.index==(mode==NET_OFFLINE?73:30));
 }
 s_net_mode=NET_GROUP;strcpy(s_draft,"Hi");save_ok=false;ui.index=69;selected=1;net_send_page();assert(ui.index==69&&!strcmp(last_notice,"SAVE ERROR"));save_ok=true;
 s_net_mode=NET_ANNOUNCEMENT;memset(s_draft,'A',128);s_draft[128]=0;selected=1;net_send_page();assert(!strcmp(last_notice,"Announcement max122"));
 s_draft[0]=0;selected=1;net_send_page();assert(!strcmp(last_notice,"Draft is empty"));
 network_on=false;s_net_mode=NET_GROUP;ui.index=71;unsigned before=sends;selected=2;net_confirm_page();assert(sends==before&&ui.index==71);
 friend_count=0;offline_recipient_page();friend_count=2;
 outbox_page();selected=5;outbox_page();assert(s_outbox_slot==5&&ui.index==73);
 entries[5].state=OUTBOX_WAITING;entries[5].rounds=120;entries[5].packet.len=128;memset(entries[5].packet.payload,'A',128);
 outbox_item_page();selected=3;outbox_item_page();assert(ui.index==74&&!deletes);
 selected=0;outbox_delete_page();assert(!deletes);selected=1;action_ok=false;outbox_delete_page();assert(!deletes);
 action_ok=true;selected=1;outbox_delete_page();assert(deletes==1);selected=2;outbox_item_page();assert(retries==1);
 storage_error=true;outbox_item_page();assert(!strcmp(outbox_state(OUTBOX_HELD),"HELD"));
 puts("PASS network action UI: 3 explicit confirmations, immutable copies, no send on preview/cancel, empty/limit/save/offline guards, outbox error/retry/delete");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-net-actions-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(shim+production+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/app'),'-I',str(root/'firmware/protocol'),str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
