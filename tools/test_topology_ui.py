"""Production topology renderer, bounded fake observations; not RF evidence."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'firmware/ui/topology_page.inc').read_text(encoding='utf8')
shim=r'''
#include "mesh_route.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct{bool enabled,paused;uint8_t last_rx_hops;uint32_t forwarded,duplicates,dropped,now;route_peer_t peers[ROUTE_PEERS];route_event_t events[ROUTE_EVENTS];}message_network_view_t;
static message_network_view_t fixture;
static unsigned s_topology_mode,s_topology_pick,lines,navigated;static bool left;
enum{BTN_ID_CW=1,BTN_ID_CC,BTN_ID_BACK,BTN_ID_SP,BTN_ID_LP,M_NETWORK_DIAGNOSTICS=97,M_NETWORK_STATUS=72};
static struct{bool pressed;unsigned id;}btn;static int u8g2,u8g2_font_5x7_tf;
static void message_service_network_view(message_network_view_t *v){*v=fixture;}
static unsigned message_service_get_channel(void){return 16;}
static void ui_navigate_to(unsigned p){navigated=p;}
static void back(void){left=true;}
static void ui_zh_draw_fit(int *p,int x,int y,const char *s,unsigned width){(void)p;assert(x>=0&&y>=0&&y<32&&s&&width<=128);}
static void text_line(int y,const char *s){assert(y>=0&&y<32&&s);}
static void u8g2_SetFont(int *p,int f){(void)p;(void)f;}
static void u8g2_SetDrawColor(int *p,int c){(void)p;assert(c==0||c==1);}
static void u8g2_DrawStr(int *p,int x,int y,const char *s){(void)p;assert(x>=0&&y>=0&&y<32&&x+strlen(s)*6<=128);}
static void u8g2_DrawHLine(int *p,int x,int y,int w){(void)p;assert(x>=0&&y>=0&&y<32&&x+w<=128);}
static void u8g2_DrawVLine(int *p,int x,int y,int h){(void)p;assert(x>=0&&x<128&&y>=0&&y+h<=32);}
static void u8g2_DrawLine(int *p,int x,int y,int a,int b){(void)p;assert(x==25&&y==15&&a==75&&(b==6||b==18));lines++;}
'''
tests=r'''
static void key(unsigned id){btn.id=id;btn.pressed=true;topology_page();}
int main(void){
 topology_page();key(BTN_ID_SP);assert(navigated==M_NETWORK_STATUS&&!fixture.enabled);
 fixture.enabled=true;fixture.now=10000;topology_page();assert(!lines);
 /* Routed origins never become neighbor edges. */
 fixture.events[0]=(route_event_t){.used=true,.hops=3,.type=MESH_TYPE_COLLAB,.rssi=-128,.at=9999,.id=0xffffffff};memset(fixture.events[0].dst,255,6);topology_page();assert(!lines);
 for(unsigned i=0;i<16;i++){fixture.peers[i].used=true;fixture.peers[i].seen=9999;fixture.peers[i].rssi=-128;strcpy(fixture.peers[i].name,"1234567890123");}
 for(unsigned i=0;i<50;i++)key(BTN_ID_CW);assert(lines>0&&s_topology_pick<16);
 key(BTN_ID_SP);assert(s_topology_mode==1);for(unsigned i=0;i<30;i++)key(BTN_ID_CC);assert(!s_topology_pick);
 for(unsigned i=1;i<8;i++)fixture.events[i]=(route_event_t){.used=true,.hops=1,.type=MESH_TYPE_RECEIPT,.at=9999-i,.id=i};
 for(unsigned i=0;i<30;i++)key(BTN_ID_CW);assert(s_topology_pick<8);
 fixture.forwarded=fixture.duplicates=fixture.dropped=0xffffffff;key(BTN_ID_SP);assert(s_topology_mode==2);key(BTN_ID_SP);assert(s_topology_mode==3);key(BTN_ID_SP);assert(s_topology_mode==0);
 fixture.now=30000;unsigned old=lines;topology_page();assert(lines==old);key(BTN_ID_LP);assert(navigated==M_NETWORK_DIAGNOSTICS);key(BTN_ID_BACK);assert(left);
 puts("PASS topology UI: beacon-only edges, separate routed origins, all modes, 16 neighbors/eight events, expiry, large counters, 128x32 bounds, readonly settings/details/back");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-topology-') as temp:
 p=Path(temp);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-I'+str(root/'firmware/protocol'),'-c',str(root/'firmware/protocol/mesh_packet.c'),'-o',str(p/'packet.o')],check=True)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),str(p/'packet.o'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True,timeout=20)
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(root/'tools/test_mesh_route.c'),str(root/'firmware/app/mesh_route.c'),str(p/'packet.o'),'-o',str(p/'route.exe')],check=True)
 subprocess.run([str(p/'route.exe')],check=True,timeout=20)
