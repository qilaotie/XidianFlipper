"""Exercise production Wi-Fi credential persistence/event logic with fake SDK/NVS."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'firmware/app/wifi_station.c').read_text(encoding='utf-8')
header = (root / 'firmware/app/wifi_station.h').read_text(encoding='utf-8')
status_type = re.search(r'typedef struct\s*\{.*?\}\s*wifi_station_status_t;', header, re.S)[0]

def function(signature):
    start = source.index(signature)
    brace = source.index('{', start)
    end, depth = brace+1, 1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}')
        end += 1
    return source[start:end]

shim = r'''
#include "wifi_credentials.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
typedef int esp_err_t;
typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NVS_NOT_FOUND 2
#define NVS_READWRITE 1
#define NVS_READONLY 0
#define WIFI_EVENT 1
#define IP_EVENT 2
#define ESP_EVENT_ANY_ID -1
#define IP_EVENT_STA_GOT_IP 3
#define WIFI_IF_STA 0
#define WIFI_AUTH_OPEN 0
#define WIFI_AUTH_WPA_PSK 2
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define ESP_LOGI(...) ((void)0)
typedef struct { uint32_t addr; } esp_ip4_addr_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(p) ((p)->addr&255),(((p)->addr>>8)&255),(((p)->addr>>16)&255),(((p)->addr>>24)&255)
typedef struct { struct { uint8_t ssid[32],password[64];struct { int authmode;int8_t rssi; } threshold;struct { bool capable,required; } pmf_cfg; } sta; } wifi_config_t;
typedef struct { uint32_t generation,ip;uint16_t reason;bool got_ip; } network_event_t;
'''
state = r'''
static wifi_station_status_t s_status;
static int s_guard;
static wifi_credentials_t s_profile,s_attempt,stored,pending;
static bool s_want_link,s_pending_save,s_have_profile,has_stored,erase_pending,s_driver,s_started;
static unsigned writes,commits,connects,s_retries;
static uint32_t s_generation,s_connect_at;
static void *s_wifi_handler,*s_ip_handler,*s_netif;
static int write_error;
static wifi_config_t last_config;
static uint32_t now_ms(void){return 100;}
static int nvs_open(const char *name,int mode,nvs_handle_t *h){assert(!strcmp(name,"wifi_user"));(void)mode;*h=1;return 0;}
static void nvs_close(nvs_handle_t h){(void)h;}
static int nvs_set_blob(nvs_handle_t h,const char *key,const void *p,size_t size){
    (void)h;assert(!strcmp(key,"profile") && size==sizeof(wifi_credentials_t));++writes;
    if(write_error)return write_error;
    memcpy(&pending,p,size);erase_pending=false;return 0;
}
static int nvs_erase_key(nvs_handle_t h,const char *key){(void)h;assert(!strcmp(key,"profile"));erase_pending=true;return 0;}
static int nvs_commit(nvs_handle_t h){(void)h;++commits;if(write_error)return write_error;if(erase_pending)has_stored=false;else {stored=pending;has_stored=true;}return 0;}
static int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
    (void)h;assert(!strcmp(key,"profile"));if(!has_stored)return ESP_ERR_NVS_NOT_FOUND;
    assert(*size==sizeof(stored));memcpy(out,&stored,sizeof(stored));return 0;
}
static void esp_event_handler_instance_unregister(int base,int id,void *h){(void)base;(void)id;(void)h;}
static void esp_wifi_stop(void){}
static void esp_wifi_deinit(void){}
static void esp_netif_destroy_default_wifi(void *p){(void)p;}
static int driver_open(void){s_driver=s_started=true;return 0;}
static int esp_wifi_set_config(int iface,const wifi_config_t *config){(void)iface;last_config=*config;return 0;}
static int esp_wifi_connect(void){++connects;return 0;}
'''
tests = r'''
int main(void){
    wifi_credentials_t good={.version=1,.security=WIFI_CRED_PSK};
    strcpy(good.ssid,"TEST_AP");strcpy(good.password,"abcdefgh");
    profile_read();assert(!s_have_profile && !s_status.saved);
    connect_profile(&good);assert(s_status.connecting && connects==1 && !writes && !s_status.saved);
    assert(!memcmp(last_config.sta.password,good.password,8));
    network_event_t event={.generation=s_generation-1,.got_ip=true,.ip=0x0101A8C0};
    handle_event(&event);assert(!s_status.online && !writes); /* Stale interface event. */
    event.generation=s_generation;handle_event(&event);
    assert(s_status.online && s_status.saved && !strcmp(s_status.ip,"192.168.1.1"));
    assert(writes==1 && commits==1 && !strcmp(stored.ssid,good.ssid));
    handle_event(&event);assert(writes==1); /* Repeated IP notifications do not wear NVS. */
    wifi_credentials_t bad=good;strcpy(bad.ssid,"OTHER_AP");strcpy(bad.password,"bad-pass");
    connect_profile(&bad);assert(!s_status.online && !s_status.ip[0]);
    event=(network_event_t){.generation=s_generation,.reason=202};
    handle_event(&event);handle_event(&event);handle_event(&event);
    assert(!s_want_link && !s_status.connecting && s_status.error);
    assert(writes==1 && !strcmp(stored.ssid,"TEST_AP")); /* Failure preserves previous profile. */
    connect_profile(&good);write_error=99;
    event=(network_event_t){.generation=s_generation,.got_ip=true,.ip=0x0201A8C0};
    handle_event(&event);assert(s_status.online && s_status.error==99);
    assert(!strcmp(stored.ssid,"TEST_AP"));
    write_error=0;assert(profile_write(NULL)==0 && !has_stored);
    puts("PASS: production save-only-after-IP, no repeated writes, failed password preserves saved profile, stale events, retry cap, NVS failure, isolated forget");
}
'''
parts = [function(sig) for sig in ['static void error_set(', 'static void profile_publish(',
         'static esp_err_t profile_write(', 'static void profile_read(', 'static void driver_close(',
         'static void connect_profile(', 'static void handle_event(']]
compiler = shutil.which('gcc')
if not compiler: raise SystemExit('Native gcc required')
with tempfile.TemporaryDirectory(prefix='wifi-state-') as folder:
    c, exe = Path(folder)/'state.c', Path(folder)/'state.exe'
    c.write_text(shim+status_type+state+'\n'.join(parts)+tests, encoding='utf-8')
    subprocess.run([compiler,'-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/app'),str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
