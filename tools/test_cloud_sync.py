"""Production sync state machine with mocked NVS/RTOS/HTTPS; not a hardware test."""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'firmware/app/cloud_sync.c').read_text(encoding='utf8')
source=re.sub(r'^#include .*\n','',source,flags=re.M)
shim=r'''
#include "cloud_sync.h"
#include "cloud_payload.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_INVALID_STATE 2
#define NVS_READWRITE 1
#define NVS_READONLY 0
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define HTTP_METHOD_POST 1
#define HTTP_METHOD_DELETE 2
typedef int esp_err_t,nvs_handle_t;
static uint8_t disk[128],staged[128];static size_t disk_size,stage_size;
static bool fail_commit,online=true;static int64_t micros=1000000;
static unsigned random_counter=1,http_calls;static int http_status=200;
static bool net_time=true;static time_t clock_value=1800000000;
static pet_t pet;
static int nvs_open(const char*n,int mode,int*h){assert(!strcmp(n,"cloud_user"));(void)mode;*h=0;return 0;}
static int nvs_get_blob(int h,const char*k,void*v,size_t*n){(void)h;assert(!strcmp(k,"state_v1"));if(!disk_size)return 1;assert(*n>=disk_size);memcpy(v,disk,disk_size);*n=disk_size;return 0;}
static int nvs_set_blob(int h,const char*k,const void*v,size_t n){(void)h;assert(!strcmp(k,"state_v1"));assert(n<=128);memcpy(staged,v,n);stage_size=n;return 0;}
static int nvs_commit(int h){(void)h;if(fail_commit)return 9;memcpy(disk,staged,stage_size);disk_size=stage_size;return 0;}
static void nvs_close(int h){(void)h;}
static void esp_fill_random(void*p,size_t n){memset(p,random_counter++,n);}
static int64_t esp_timer_get_time(void){return micros;}
typedef struct{bool online;}wifi_station_status_t;
static void wifi_station_status(wifi_station_status_t*s){s->online=online;}
static bool pet_service_error(void){return false;}
static const pet_t *pet_service_state(void){return &pet;}
static const char *nvs_config_get_device_name(void){return "Tester";}
static bool arcade_record_best(unsigned k,uint32_t*s,uint32_t*p){*s=k?1:120;*p=k?4:1;return true;}
typedef struct{size_t size;bool full;unsigned char bytes[1024];}queue_t;
typedef queue_t* QueueHandle_t;
static QueueHandle_t xQueueCreate(unsigned count,size_t size){assert(count==1&&size<=1024);queue_t*q=calloc(1,sizeof(*q));q->size=size;return q;}
static void vQueueDelete(QueueHandle_t q){free(q);}
static int xQueueOverwrite(QueueHandle_t q,const void*v){memcpy(q->bytes,v,q->size);q->full=true;return 1;}
static int xQueueReceive(QueueHandle_t q,void*v,unsigned timeout){(void)timeout;if(!q->full)return 0;memcpy(v,q->bytes,q->size);q->full=false;return 1;}
static int xTaskCreate(void(*fn)(void*),const char*n,unsigned stack,void*a,unsigned priority,void*h){(void)fn;(void)n;(void)a;(void)priority;(void)h;assert(stack>=8192);return 1;}
static time_t fake_time(time_t*t){if(t)*t=clock_value;return clock_value;}
#define time fake_time
typedef struct{const char*server;}esp_sntp_config_t;
#define ESP_NETIF_SNTP_DEFAULT_CONFIG(x) {.server=(x)}
static int esp_netif_sntp_init(const esp_sntp_config_t*c){assert(!strcmp(c->server,"time.cloudflare.com"));return 0;}
static int esp_netif_sntp_sync_wait(unsigned timeout){assert(timeout==15000);if(!net_time)return 9;clock_value=1800000000;return 0;}
static int esp_crt_bundle_attach(void*p){(void)p;return 0;}
typedef struct{const char*url;const char*user_agent;int method;int(*crt_bundle_attach)(void*);int timeout_ms;bool disable_auto_redirect;int buffer_size,buffer_size_tx;}esp_http_client_config_t;
typedef void* esp_http_client_handle_t;
static char auth_seen[80],body_seen[640];static int method_seen;
static void*esp_http_client_init(const esp_http_client_config_t*c){assert(!strncmp(c->url,"https://",8));assert(c->crt_bundle_attach==esp_crt_bundle_attach);assert(c->disable_auto_redirect);method_seen=c->method;return (void*)1;}
static int esp_http_client_set_header(void*c,const char*k,const char*v){(void)c;if(!strcmp(k,"Authorization"))snprintf(auth_seen,sizeof(auth_seen),"%s",v);return 0;}
static int esp_http_client_set_post_field(void*c,const char*v,int n){(void)c;assert((size_t)n<sizeof(body_seen));memcpy(body_seen,v,n+1);return 0;}
static int esp_http_client_perform(void*c){(void)c;http_calls++;return 0;}
static int esp_http_client_get_status_code(void*c){(void)c;return http_status;}
static void esp_http_client_cleanup(void*c){(void)c;}
static void mbedtls_platform_zeroize(void*p,size_t n){memset(p,0,n);}
'''
tests=r'''
static void finish(void){job_t job;assert(xQueueReceive(jobs,&job,0));result_t r={job.generation,transmit(&job)};xQueueOverwrite(results,&r);}
static void reboot(void){if(jobs)vQueueDelete(jobs);if(results)vQueueDelete(results);jobs=results=NULL;initialized=blocked=busy=false;memset(&settings,0,sizeof(settings));atomic_store(&generation,0);due=0;failures=0;code=0;}
int main(void){
 pet_new(&pet);cloud_sync_tick();assert(!settings.enabled&&!http_calls&&!jobs&&!disk_size);
 assert(!cloud_sync_now());fail_commit=true;assert(!cloud_sync_enable(true));cloud_sync_tick();assert(blocked&&!http_calls);
 fail_commit=false;reboot();assert(cloud_sync_enable(true));assert(disk_size==52&&identity(&settings));
 uint8_t id[16],token[32];memcpy(id,settings.id,16);memcpy(token,settings.token,32);
 cloud_sync_tick();assert(busy);finish();cloud_sync_tick();assert(code==200&&!busy&&settings.enabled&&http_calls==1);
 assert(strlen(auth_seen)==71&&!strncmp(auth_seen,"Bearer ",7));assert(strstr(body_seen,"\"nickname\":\"Tester\""));
 cloud_sync_tick();assert(!busy&&http_calls==1);reboot();assert(settings.version==0);cloud_sync_tick();assert(settings.enabled&&busy);
 assert(!memcmp(settings.id,id,16)&&!memcmp(settings.token,token,32));
 assert(cloud_sync_enable(false));unsigned calls=http_calls;finish();cloud_sync_tick();assert(!busy&&http_calls==calls);
 assert(!cloud_sync_now());assert(cloud_sync_remove());online=false;cloud_sync_tick();assert(settings.remove&&!busy);
 reboot();cloud_sync_tick();assert(settings.remove&&!settings.enabled&&!busy);
 assert(!cloud_sync_enable(true));online=true;cloud_sync_tick();assert(busy);finish();cloud_sync_tick();
 assert(!settings.remove&&!settings.enabled&&method_seen==HTTP_METHOD_DELETE&&code==200);
 assert(cloud_sync_enable(true));cloud_sync_tick();http_status=429;finish();cloud_sync_tick();assert(code==429&&!busy&&due>micros);
 unsigned before=http_calls;cloud_sync_tick();assert(http_calls==before);
 micros=due+1;cloud_sync_tick();assert(busy);assert(cloud_sync_remove());finish();cloud_sync_tick();assert(busy);
 http_status=200;finish();cloud_sync_tick();assert(!settings.enabled&&!settings.remove);
 assert(cloud_sync_enable(true));clock_value=0;net_time=false;cloud_sync_tick();before=http_calls;finish();cloud_sync_tick();assert(code==-2&&http_calls==before);
 net_time=true;clock_value=1800000000;assert(cloud_sync_now());cloud_sync_tick();finish();cloud_sync_tick();assert(code==200);
 reboot();disk[0]=99;cloud_sync_tick();assert(blocked&&!cloud_sync_enable(true));
 puts("PASS cloud owner/state: default OFF, persist-before-upload, stable identity, cancellation, remove/reboot/offline retry, cooldown, TLS/time gate, corrupt-record preservation");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-cloud-sync-') as tmp:
    tmp=Path(tmp);(tmp/'test.c').write_text(shim+source+tests,encoding='utf8')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'firmware/app'),str(tmp/'test.c'),
                    str(root/'firmware/app/cloud_payload.c'),str(root/'firmware/app/pet.c'),'-o',str(tmp/'test.exe')],check=True)
    subprocess.run([str(tmp/'test.exe')],check=True)
