"""Production experiment/gift UI: confirmation snapshots and no preview side effects."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'firmware/ui/extras_pages.inc').read_text(encoding='utf8')
shim=r'''
#include "pet.h"
#include "pet_gifts.h"
#include "network_lab.h"
#include "mesh_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
typedef struct{const char *m_select;}M_SELECT;
enum{M_NETWORK_LAB=122,M_PET_GIFTS=123,M_NETWORK_STATUS=72};
typedef struct{uint32_t now;route_peer_t peers[ROUTE_PEERS];}message_network_view_t;
static message_network_view_t net;static network_lab_view_t lv;static pet_gifts_t gv;static pet_t pet;
static struct{unsigned layer,select[8];}ui;static bool s_draft_list_enter;
static int selected=-1;static unsigned sends,decisions,cancels,starts,stops,navigated,rendered,last_amount,last_asset;static uint8_t last_peer[6];static const char *last_notice;
static const char *ui_zh_lookup(const char *s){return s;}
static int menu(M_SELECT *rows,unsigned n){assert(n&&n<=24);for(unsigned i=0;i<n;i++)assert(rows[i].m_select&&strlen(rows[i].m_select)<128);rendered=n;int result=selected;selected=-1;return result;}
static void notice(const char *s){last_notice=s;}
static void ui_navigate_to(unsigned page){navigated=page;}
static void message_service_network_view(message_network_view_t *v){*v=net;}
static void message_service_lab_view(network_lab_view_t *v){*v=lv;}
static bool message_service_lab_start(const uint8_t *peer,unsigned preset){assert(peer&&preset<3);if(!peer[5])return false;starts++;lv.running=true;memcpy(last_peer,peer,6);return true;}
static void message_service_lab_stop(void){stops++;lv.running=false;}
static pet_gifts_t *gift_runtime_state(void){return &gv;}
static const pet_t *pet_service_state(void){return &pet;}
static bool pet_service_error(void){return false;}
bool pet_gifts_offer(pet_gifts_t *s,const uint8_t *peer,unsigned asset,unsigned n){(void)s;sends++;last_amount=n;last_asset=asset;memcpy(last_peer,peer,6);return true;}
bool pet_gifts_decide(pet_gifts_t *s,bool accept){(void)s;(void)accept;decisions++;return true;}
bool pet_gifts_cancel(pet_gifts_t *s){(void)s;cancels++;return true;}
'''
tests=r'''
int main(void){
 gv.enabled=gv.armed=true;gv.channel=1;gv.scope=77;gv.now=5000;gv.peers[0]=(gift_peer_t){.used=true,.seen=4999,.mac={2,0,0,0,0,2},.name="Alice"};
 extras_pages_enter(M_PET_GIFTS);gift_page();assert(rendered==11&&!sends&&!decisions);selected=10;gift_page();selected=3;gift_page();assert(gift_confirm==1&&ui.select[0]==0&&!sends);
 gift_choice=2;gift_target[5]=9;selected=3;gift_page();assert(sends==1&&last_amount==10&&last_asset==0&&last_peer[5]==2);
 selected=3;gift_page();selected=0;gift_page();assert(sends==1&&!gift_confirm);
 selected=3;gift_page();gv.scope++;selected=3;gift_page();assert(sends==1&&last_notice);gift_confirm=0;gv.scope--;
 gv.offer=true;gv.incoming=(gift_tx_t){.peer={2,0,0,0,0,2},.channel=1,.scope=77,.amount=10,.id=5};selected=5;gift_page();assert(gift_confirm==2&&!decisions);gv.incoming.id++;selected=3;gift_page();assert(!decisions);gift_confirm=0;
 selected=6;gift_page();assert(gift_confirm==3&&!decisions);selected=3;gift_page();assert(decisions==1);
 gv.book.outgoing=(gift_tx_t){.peer={2,0,0,0,0,2},.channel=1,.scope=77,.amount=10,.id=4,.stage=GIFT_OFFERING};selected=4;gift_page();assert(gift_confirm==4&&!cancels);selected=3;gift_page();assert(cancels==1);
 net.now=5000;for(unsigned i=0;i<16;i++){net.peers[i]=(route_peer_t){.used=true,.seen=4999,.mac={2,0,0,0,0,i+1}};strcpy(net.peers[i].name,"1234567890123");}extras_pages_enter(M_NETWORK_LAB);lab_page();assert(rendered==24&&!starts);selected=8;lab_page();assert(!starts);selected=6;lab_page();assert(starts==1&&last_peer[5]==1);selected=9;lab_page();assert(lab_target[5]==1);selected=6;lab_page();assert(stops==1);
 selected=0;lab_page();assert(navigated==M_NETWORK_STATUS);net.now=30000;selected=7;lab_page();lab_page();assert(rendered==8);
 puts("PASS extras UI: no send/debit on preview, cancel default, immutable peer/asset/amount, stale scope/offer guard, accept/decline/cancel confirmation, bounded peer lists and explicit experiment start/stop");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-extras-ui-') as temp:
 p=Path(temp);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True,timeout=30)
