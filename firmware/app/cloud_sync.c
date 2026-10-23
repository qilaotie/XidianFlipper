#include "cloud_sync.h"
#include "cloud_payload.h"
#include "arcade_store.h"
#include "pet_service.h"
#include "wifi_station.h"
#include "nvs_config.h"
#include "nvs.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mbedtls/platform_util.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CLOUD_URL "https://xidianflipper-api.pages.dev/v1/sync"
typedef struct {uint8_t version,enabled,remove,reserved,id[16],token[32];} settings_t;
typedef struct {unsigned generation;bool remove;char token[65],body[640];} job_t;
typedef struct {unsigned generation;int result;} result_t;
static settings_t settings;
static bool initialized,blocked,busy;
static QueueHandle_t jobs,results;
static atomic_uint generation;
static int code;
static unsigned failures;
static int64_t due;
static const char *text="SHARING OFF";

static bool identity(const settings_t *s){
    unsigned id=0,token=0;for(unsigned i=0;i<16;i++)id|=s->id[i];for(unsigned i=0;i<32;i++)token|=s->token[i];
    return id && token;
}
static bool persist(const settings_t *next){
    nvs_handle_t h;esp_err_t rc=nvs_open("cloud_user",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"state_v1",next,sizeof(*next));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    if(rc!=ESP_OK){blocked=true;text="CLOUD SAVE ERROR";atomic_fetch_add(&generation,1);return false;}
    settings=*next;return true;
}
static void init(void){
    if(initialized)return;
    initialized=true;settings.version=1;
    nvs_handle_t h;settings_t saved={0};size_t size=sizeof(saved);esp_err_t rc=nvs_open("cloud_user",NVS_READONLY,&h);
    if(rc==ESP_OK){rc=nvs_get_blob(h,"state_v1",&saved,&size);nvs_close(h);}
    if(rc==ESP_OK){
        if(size!=sizeof(saved)||saved.version!=1||saved.enabled>1||saved.remove>1||saved.reserved||
           (saved.enabled&&saved.remove)||!identity(&saved)){blocked=true;text="CLOUD SAVE ERROR";}
        else {settings=saved;text=settings.remove?"REMOVE PENDING":settings.enabled?"WAIT FOR WI-FI":"SHARING OFF";}
    }else if(rc!=ESP_ERR_NVS_NOT_FOUND){blocked=true;text="CLOUD SAVE ERROR";}
}
static int transmit(const job_t *job){
    wifi_station_status_t wifi;wifi_station_status(&wifi);if(!wifi.online)return -1;
    if(atomic_load(&generation)!=job->generation)return -3;
    static bool clock_started;
    if(time(NULL)<1704067200){
        if(!clock_started){esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG("time.cloudflare.com");
            esp_err_t rc=esp_netif_sntp_init(&config);if(rc==ESP_OK)clock_started=true;else return -2;}
        if(esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000))!=ESP_OK || time(NULL)<1704067200)return -2;
    }
    if(atomic_load(&generation)!=job->generation)return -3;
    esp_http_client_config_t config={.url=CLOUD_URL,.user_agent="XidianFlipper/1.0",.method=job->remove?HTTP_METHOD_DELETE:HTTP_METHOD_POST,
        .crt_bundle_attach=esp_crt_bundle_attach,.timeout_ms=12000,.disable_auto_redirect=true,
        .buffer_size=1024,.buffer_size_tx=1024};
    esp_http_client_handle_t client=esp_http_client_init(&config);if(!client)return -4;
    char auth[73];snprintf(auth,sizeof(auth),"Bearer %s",job->token);
    esp_err_t rc=esp_http_client_set_header(client,"Authorization",auth);
    if(rc==ESP_OK)rc=esp_http_client_set_header(client,"Content-Type","application/json");
    if(rc==ESP_OK)rc=esp_http_client_set_post_field(client,job->body,(int)strlen(job->body));
    if(rc==ESP_OK && atomic_load(&generation)==job->generation)rc=esp_http_client_perform(client);
    else if(rc==ESP_OK)rc=ESP_ERR_INVALID_STATE;
    int result=rc==ESP_OK?esp_http_client_get_status_code(client):-4;
    esp_http_client_cleanup(client);mbedtls_platform_zeroize(auth,sizeof(auth));return result;
}
static void worker(void *arg){
    (void)arg;job_t job;
    while(1)if(xQueueReceive(jobs,&job,portMAX_DELAY)==pdTRUE){
        result_t result={job.generation,transmit(&job)};
        mbedtls_platform_zeroize(&job,sizeof(job));xQueueOverwrite(results,&result);
    }
}
static bool start(void){
    if(jobs)return true;
    jobs=xQueueCreate(1,sizeof(job_t));results=xQueueCreate(1,sizeof(result_t));
    /* TLS arithmetic can run longer than the idle watchdog interval on C6.
     * Share the idle priority's time slices rather than starving IDLE0; never
     * disable its watchdog or certificate verification to hide that starvation. */
    if(jobs && results && xTaskCreate(worker,"cloud_sync",8192,NULL,0,NULL)==pdPASS)return true;
    if(jobs)vQueueDelete(jobs);
    if(results)vQueueDelete(results);
    jobs=results=NULL;text="NO MEMORY / RETRY";return false;
}
bool cloud_sync_enable(bool enabled){
    init();if(blocked || (enabled&&settings.remove))return false;
    if(settings.enabled==enabled)return true;
    settings_t next=settings;next.enabled=enabled;
    if(!identity(&next)){esp_fill_random(next.id,sizeof(next.id));esp_fill_random(next.token,sizeof(next.token));}
    if(!persist(&next))return false;
    atomic_fetch_add(&generation,1);due=0;failures=0;code=0;text=enabled?"WAIT FOR WI-FI":"SHARING OFF";return true;
}
bool cloud_sync_now(void){init();if(blocked || (!settings.enabled&&!settings.remove))return false;due=0;return true;}
bool cloud_sync_remove(void){
    init();if(blocked)return false;if(!identity(&settings)){text="NOT SHARED YET";return true;}
    settings_t next=settings;next.enabled=0;next.remove=1;
    if(!persist(&next))return false;
    atomic_fetch_add(&generation,1);due=0;failures=0;code=0;text="REMOVE PENDING";return true;
}
void cloud_sync_tick(void){
    init();int64_t now=esp_timer_get_time();result_t result;
    if(results && xQueueReceive(results,&result,0)==pdTRUE){
        busy=false;
        if(result.generation==atomic_load(&generation)){
            code=result.result;
            if(code==200){
                failures=0;due=now+900000000;text="SYNC OK / AUTO 15M";
                if(settings.remove){settings_t next=settings;next.remove=0;if(persist(&next))text="REMOVED / SHARE OFF";}
            }else{if(failures<4)failures++;due=now+(int64_t)(60u<<(failures-1))*1000000;
                text=code==-2?"WAIT FOR NET TIME":code==-1?"WAIT FOR WI-FI":code==401?"CREDENTIAL REJECTED":code==429?"RATE LIMIT / RETRY":"NETWORK ERROR / RETRY";}
        }
    }
    if(blocked || busy || (!settings.enabled&&!settings.remove) || now<due)return;
    wifi_station_status_t wifi;wifi_station_status(&wifi);if(!wifi.online){if(code==0)text=settings.remove?"REMOVE: NEED WI-FI":"WAIT FOR WI-FI";return;}
    if(!start()){due=now+60000000;return;}
    job_t job={.generation=atomic_load(&generation),.remove=settings.remove};char id[33];cloud_hex(id,settings.id,16);cloud_hex(job.token,settings.token,32);
    if(job.remove)snprintf(job.body,sizeof(job.body),"{\"id\":\"%s\"}",id);
    else {
        cloud_score_t scores[2]={0};
        for(unsigned k=0;k<2;k++)scores[k].present=arcade_record_best(k,&scores[k].score,&scores[k].progress);
        if(pet_service_error() || !cloud_payload(job.body,sizeof(job.body),id,nvs_config_get_device_name(),pet_service_state(),scores)){
            text="LOCAL DATA ERROR";due=now+60000000;mbedtls_platform_zeroize(&job,sizeof(job));return;}
    }
    if(xQueueOverwrite(jobs,&job)==pdTRUE){busy=true;text=job.remove?"REMOVING ONLINE...":"SYNCING HTTPS...";}
    mbedtls_platform_zeroize(&job,sizeof(job));
}
void cloud_sync_status(cloud_sync_status_t *out){
    init();*out=(cloud_sync_status_t){.enabled=settings.enabled,.pending_remove=settings.remove,.busy=busy,.storage_error=blocked,.result=code,.text=text};
    if(identity(&settings)){char id[33];cloud_hex(id,settings.id,16);memcpy(out->id,id,6);out->id[6]=0;}
}
