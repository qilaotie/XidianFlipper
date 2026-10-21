"""Host test of production key pulse/release functions, with the GATT send mocked."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'firmware/radio/tool_hid.c').read_text(encoding='utf-8')

def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

shim = r'''
#include "tool_hid.h"
#include "tool_hid_report.h"
#include <assert.h>
#include <stdio.h>
#define BLE_HS_CONN_HANDLE_NONE 65535
#define BLE_HS_ENOMEM 6
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
static int s_lock;
static tool_hid_status_t s_status={.enabled=true,.connected=true,.encrypted=true,.keyboard_ready=true,.media_ready=true};
static uint16_t s_conn=7,s_kbd_handle=10,s_media_handle=20,s_release_conn;
static bool s_release_pending,s_release_media,s_advertising=true;
static int64_t clock_us=1000000,s_release_at,s_adv_attempt;
struct os_mbuf { uint8_t bytes[8];unsigned size; };
static struct os_mbuf mbuf;
static unsigned sends,failures,last_size;static uint8_t last[8];static uint16_t last_handle;
static int64_t esp_timer_get_time(void){return clock_us;}
static void tool_hid_status(tool_hid_status_t *out){*out=s_status;}
static bool advertise(void){return true;}
static struct os_mbuf *ble_hs_mbuf_from_flat(const void *data,unsigned size){
    memcpy(mbuf.bytes,data,size);mbuf.size=size;return &mbuf;
}
static int ble_gatts_notify_custom(uint16_t conn,uint16_t handle,struct os_mbuf *data){
    assert(conn==s_conn);
    if(failures){--failures;return 1;}
    ++sends;last_handle=handle;last_size=data->size;memcpy(last,data->bytes,last_size);return 0;
}
'''.replace('static void tool_hid_status', 'void tool_hid_status')
tests = r'''
int main(void){
    assert(tool_hid_press_for(true,0xE9,120));assert(sends==1 && last_handle==20 && last_size==2 && last[0]==0xE9);
    assert(!tool_hid_press_for(true,0xEA,120));
    clock_us+=119999;tool_hid_tick();assert(sends==1 && s_release_pending);
    ++clock_us;failures=1;tool_hid_tick();assert(sends==1 && s_release_pending);
    tool_hid_tick();assert(sends==2 && !s_release_pending && last[0]==0 && last[1]==0);
    assert(tool_hid_press(false,0x2C));assert(last_handle==10 && last_size==8 && last[2]==0x2C);
    clock_us+=44999;tool_hid_tick();assert(s_release_pending);
    ++clock_us;tool_hid_tick();assert(!s_release_pending);
    for(unsigned i=0;i<8;i++)assert(last[i]==0);
    s_status.media_ready=false;assert(!tool_hid_press_for(true,0xE9,120));
    assert(tool_hid_press_for(false,0x28,80));
    s_status.connected=false;s_conn=BLE_HS_CONN_HANDLE_NONE;
    clock_us+=80000;unsigned before=sends;tool_hid_tick();assert(!s_release_pending && sends==before);
    s_status.connected=true;s_conn=7;s_status.encrypted=false;
    assert(!tool_hid_press(false,0x2C));
    puts("PASS: production pulse deadline, release retries/zeros, busy guard, default 45ms, subscription/encryption guard, disconnect");
}
'''
compiler = shutil.which('gcc')
if not compiler:
    raise SystemExit('Native gcc required')
parts = [function(s) for s in ['static int notify(', 'bool tool_hid_press(', 'bool tool_hid_press_for(', 'void tool_hid_tick(']]
with tempfile.TemporaryDirectory(prefix='hid-pulse-') as folder:
    c, exe = Path(folder)/'pulse.c', Path(folder)/'pulse.exe'
    c.write_text(shim + '\n'.join(parts) + tests, encoding='utf-8')
    subprocess.run([compiler,'-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/radio'),str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
