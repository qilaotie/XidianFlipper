"""Real network-page renderer with bounded fake snapshots; not RF validation."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
local=(root/'firmware/ui/local_pages.c').read_text(encoding='utf8')
source=local[local.index('static void network_page(void)'):local.index('#include "team_pages.inc"')]
shim=r'''
#include "mesh_route.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct{bool enabled,paused;uint8_t last_rx_hops;uint32_t forwarded,duplicates,dropped,now;route_peer_t peers[ROUTE_PEERS];}message_network_view_t;
static message_network_view_t fixture;
static unsigned s_network_page;
enum{BTN_ID_CW=1,BTN_ID_CC,BTN_ID_BACK,BTN_ID_SP};
static struct{bool pressed;int id;}btn;
static int u8g2,u8g2_font_5x7_tf;static bool left;
static bool ui_language_chinese(void){return false;}
static const char *ui_zh_lookup(const char *s){return s;}
static void ui_zh_draw_fit(int *p,int x,int y,const char *s,unsigned w){(void)p;(void)x;(void)y;assert(s&&w<=128);}
static void message_service_network_view(message_network_view_t *v){*v=fixture;}
static bool message_service_network_enable(bool b){fixture.enabled=b;return true;}
static unsigned message_service_get_channel(void){return 16;}
static void back(void){left=true;}
static void u8g2_SetDrawColor(int *p,int n){(void)p;(void)n;}
static void u8g2_SetFont(int *p,int n){(void)p;(void)n;}
static void u8g2_DrawStr(int *p,int x,int y,const char *s){(void)p;assert(x>=0&&x+(int)strlen(s)*5<=128&&y>=7&&y<=31);}
'''
tests=r'''
int main(void){
 fixture.now=10000;fixture.forwarded=fixture.dropped=fixture.duplicates=0xffffffff;
 network_page();btn.id=BTN_ID_SP;btn.pressed=true;network_page();assert(fixture.enabled);
 fixture.paused=true;network_page();fixture.last_rx_hops=3;
 for(unsigned i=0;i<16;i++){fixture.peers[i].used=true;fixture.peers[i].seen=1;fixture.peers[i].rssi=-128;memset(fixture.peers[i].mac,255,6);strcpy(fixture.peers[i].name,"1234567890123");}
 for(unsigned i=0;i<100;i++){btn.id=BTN_ID_CW;btn.pressed=true;network_page();assert(s_network_page<18);}
 fixture.now=20000;s_network_page=17;network_page();assert(s_network_page==0);
 btn.id=BTN_ID_CC;btn.pressed=true;network_page();assert(s_network_page==1);
 btn.id=BTN_ID_BACK;btn.pressed=true;network_page();assert(left);
 puts("PASS network UI: toggle, 16 peers, max counters/ASCII, pause, expiry, wrap, back, 128x32 bounds");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-net-ui-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/app'),'-I',str(root/'firmware/protocol'),str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
