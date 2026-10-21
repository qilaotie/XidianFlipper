/* Sole Wi-Fi driver owner for both scan and connection pages. Credentials never
 * enter status snapshots or logs. Saved profiles use a separate NVS namespace. */
#include "wifi_station.h"
#include "wifi_credentials.h"
#include "message_service.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { W_SCAN=1,W_CONNECT,W_SAVED,W_DISCONNECT,W_FORGET,W_AUTO,W_IDLE };
typedef struct { uint8_t type,value;wifi_credentials_t credentials; } command_t;
typedef struct { uint32_t generation,ip;uint16_t reason;bool got_ip; } network_event_t;
static portMUX_TYPE s_guard=portMUX_INITIALIZER_UNLOCKED;
static wifi_station_status_t s_status;
static wifi_ap_record_t s_aps[WIFI_STATION_AP_MAX];
static QueueHandle_t s_commands,s_events;
static esp_netif_t *s_netif;
static esp_event_handler_instance_t s_wifi_handler,s_ip_handler;
static bool s_driver,s_started,s_tcpip,s_want_link,s_have_profile,s_pending_save;
static uint32_t s_generation,s_connect_at;static unsigned s_retries;
static wifi_credentials_t s_profile,s_attempt;
static uint32_t now_ms(void){return (uint32_t)(esp_timer_get_time()/1000);}
static void error_set(int error){portENTER_CRITICAL(&s_guard);s_status.error=error;portEXIT_CRITICAL(&s_guard);}
static void profile_publish(void){
    portENTER_CRITICAL(&s_guard);
    s_status.saved=s_have_profile;s_status.automatic=s_have_profile && s_profile.automatic;
    snprintf(s_status.saved_ssid,sizeof(s_status.saved_ssid),"%s",s_have_profile?s_profile.ssid:"");
    portEXIT_CRITICAL(&s_guard);
}
static esp_err_t profile_write(const wifi_credentials_t *profile){
    nvs_handle_t handle;esp_err_t rc=nvs_open("wifi_user",NVS_READWRITE,&handle);
    if(rc!=ESP_OK)return rc;
    rc=profile?nvs_set_blob(handle,"profile",profile,sizeof(*profile)):nvs_erase_key(handle,"profile");
    if(rc==ESP_ERR_NVS_NOT_FOUND && !profile)rc=ESP_OK;
    if(rc==ESP_OK)rc=nvs_commit(handle);
    nvs_close(handle);return rc;
}
static void profile_read(void){
    nvs_handle_t handle;wifi_credentials_t profile={0};size_t size=sizeof(profile);
    if(nvs_open("wifi_user",NVS_READONLY,&handle)==ESP_OK){
        esp_err_t rc=nvs_get_blob(handle,"profile",&profile,&size);nvs_close(handle);
        if(rc==ESP_OK && size==sizeof(profile) && wifi_credentials_valid(&profile)){s_profile=profile;s_have_profile=true;}
    }
    memset(&profile,0,sizeof(profile));profile_publish();
}
static void network_event(void *arg,esp_event_base_t base,int32_t id,void *data){
    network_event_t event={.generation=(uint32_t)(uintptr_t)arg};
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP){event.got_ip=true;event.ip=((ip_event_got_ip_t*)data)->ip_info.ip.addr;}
    else if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED)event.reason=((wifi_event_sta_disconnected_t*)data)->reason;
    else return;
    /* Bounded event mailbox; the connection deadline also covers lost events. */
    xQueueSend(s_events,&event,0);
}
static void driver_close(void){
    ++s_generation;s_want_link=false;s_pending_save=false;
    if(s_wifi_handler){esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,s_wifi_handler);s_wifi_handler=NULL;}
    if(s_ip_handler){esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,s_ip_handler);s_ip_handler=NULL;}
    if(s_started){esp_wifi_stop();s_started=false;}
    if(s_driver){esp_wifi_deinit();s_driver=false;}
    if(s_netif){esp_netif_destroy_default_wifi(s_netif);s_netif=NULL;}
    memset(&s_attempt,0,sizeof(s_attempt));
    portENTER_CRITICAL(&s_guard);
    s_status.initialized=s_status.online=s_status.connecting=false;s_status.ip[0]=0;s_status.rssi=0;
    portEXIT_CRITICAL(&s_guard);
}
static esp_err_t driver_open(void){
    if(s_driver && s_started)return ESP_OK;
    esp_err_t rc;
    if(!s_tcpip){rc=esp_netif_init();if(rc!=ESP_OK)return rc;s_tcpip=true;}
    rc=esp_event_loop_create_default();if(rc!=ESP_OK && rc!=ESP_ERR_INVALID_STATE)return rc;
    esp_netif_config_t netif_cfg=ESP_NETIF_DEFAULT_WIFI_STA();
    s_netif=esp_netif_new(&netif_cfg);if(!s_netif)return ESP_ERR_NO_MEM;
    rc=esp_netif_attach_wifi_station(s_netif);
    if(rc==ESP_OK)rc=esp_wifi_set_default_wifi_sta_handlers();
    if(rc!=ESP_OK)goto fail;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    rc=esp_wifi_init(&init);if(rc!=ESP_OK)goto fail;s_driver=true;
    rc=esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if(rc==ESP_OK)rc=esp_wifi_set_mode(WIFI_MODE_STA);
    if(rc==ESP_OK)rc=esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,network_event,(void*)(uintptr_t)s_generation,&s_wifi_handler);
    if(rc==ESP_OK)rc=esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,network_event,(void*)(uintptr_t)s_generation,&s_ip_handler);
    if(rc==ESP_OK)rc=esp_wifi_start();
    if(rc!=ESP_OK)goto fail;
    s_started=true;esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    portENTER_CRITICAL(&s_guard);s_status.initialized=true;portEXIT_CRITICAL(&s_guard);
    return ESP_OK;
fail:driver_close();return rc;
}
static void connect_profile(const wifi_credentials_t *profile){
    driver_close();
    if(!wifi_credentials_valid(profile)){error_set(ESP_ERR_INVALID_ARG);return;}
    esp_err_t rc=driver_open();
    if(rc!=ESP_OK){error_set(rc);return;}
    wifi_config_t config={0};
    memcpy(config.sta.ssid,profile->ssid,strlen(profile->ssid));
    memcpy(config.sta.password,profile->password,strlen(profile->password));
    config.sta.threshold.authmode=profile->security==WIFI_CRED_OPEN?WIFI_AUTH_OPEN:WIFI_AUTH_WPA_PSK;
    config.sta.threshold.rssi=-127;
    config.sta.pmf_cfg.capable=true;config.sta.pmf_cfg.required=false;
    rc=esp_wifi_set_config(WIFI_IF_STA,&config);memset(&config,0,sizeof(config));
    if(rc==ESP_OK){
        s_attempt=*profile;s_pending_save=true;s_want_link=true;s_retries=0;s_connect_at=now_ms();
        portENTER_CRITICAL(&s_guard);
        s_status.connecting=true;s_status.online=false;s_status.error=0;s_status.reason=0;
        snprintf(s_status.ssid,sizeof(s_status.ssid),"%s",profile->ssid);
        portEXIT_CRITICAL(&s_guard);
        rc=esp_wifi_connect();
    }
    if(rc!=ESP_OK){driver_close();error_set(rc);}
}
static void scan(void){
    wifi_station_status_t status;wifi_station_status(&status);
    bool lease=false;esp_err_t rc=ESP_OK;
    if(status.connecting){rc=ESP_ERR_INVALID_STATE;goto done;}
    if(!status.online){
        for(unsigned attempt=0;attempt<12;attempt++){
            lease=message_service_pause_for_tools();if(lease)break;vTaskDelay(pdMS_TO_TICKS(250));
        }
        if(!lease){rc=ESP_ERR_INVALID_STATE;goto done;}
    }
    rc=driver_open();if(rc!=ESP_OK)goto done;
    wifi_scan_config_t config={.show_hidden=true,.scan_type=WIFI_SCAN_TYPE_PASSIVE,.scan_time.passive=150};
    rc=esp_wifi_scan_start(&config,true);
    if(rc==ESP_OK){
        uint16_t count=WIFI_STATION_AP_MAX;
        wifi_ap_record_t *records=calloc(count,sizeof(*records));
        if(!records){esp_wifi_clear_ap_list();rc=ESP_ERR_NO_MEM;}
        else{
            rc=esp_wifi_scan_get_ap_records(&count,records);
            if(rc==ESP_OK){portENTER_CRITICAL(&s_guard);memcpy(s_aps,records,count*sizeof(*records));s_status.count=count;portEXIT_CRITICAL(&s_guard);}
            free(records);
        }
    }
done:
    if(lease)message_service_resume();
    portENTER_CRITICAL(&s_guard);s_status.scanning=false;s_status.error=rc;portEXIT_CRITICAL(&s_guard);
    ESP_LOGI("wifi_user","scan complete: %s",esp_err_to_name(rc));
}
static void handle_event(const network_event_t *event){
    if(event->generation!=s_generation || !s_want_link)return;
    if(event->got_ip){
        esp_ip4_addr_t ip={.addr=event->ip};
        portENTER_CRITICAL(&s_guard);
        s_status.online=true;s_status.connecting=false;s_status.error=0;s_status.reason=0;
        snprintf(s_status.ip,sizeof(s_status.ip),IPSTR,IP2STR(&ip));
        portEXIT_CRITICAL(&s_guard);
        if(s_pending_save){
            esp_err_t rc=profile_write(&s_attempt);
            if(rc==ESP_OK){s_profile=s_attempt;s_have_profile=true;profile_publish();}else error_set(rc);
            s_pending_save=false;
        }
        s_retries=0;ESP_LOGI("wifi_user","station acquired IPv4 address");
    }else{
        portENTER_CRITICAL(&s_guard);
        s_status.online=false;s_status.ip[0]=0;s_status.reason=event->reason;
        portEXIT_CRITICAL(&s_guard);
        if(s_retries++<2){
            s_connect_at=now_ms();
            portENTER_CRITICAL(&s_guard);s_status.connecting=true;portEXIT_CRITICAL(&s_guard);
            esp_err_t rc=esp_wifi_connect();if(rc!=ESP_OK){driver_close();error_set(rc);}
        }else {driver_close();error_set(ESP_FAIL);}
    }
}
static void worker(void *unused){
    (void)unused;profile_read();
    if(s_have_profile && s_profile.automatic)connect_profile(&s_profile);
    for(;;){
        command_t command;
        if(xQueueReceive(s_commands,&command,pdMS_TO_TICKS(100))==pdTRUE){
            switch(command.type){
            case W_SCAN:scan();break;
            case W_CONNECT:connect_profile(&command.credentials);break;
            case W_SAVED:if(s_have_profile)connect_profile(&s_profile);else error_set(ESP_ERR_NOT_FOUND);break;
            case W_DISCONNECT:driver_close();error_set(0);break;
            case W_FORGET:{
                driver_close();esp_err_t rc=profile_write(NULL);
                if(rc==ESP_OK){memset(&s_profile,0,sizeof(s_profile));s_have_profile=false;profile_publish();}
                error_set(rc);break;
            }
            case W_AUTO:if(s_have_profile){
                wifi_credentials_t changed=s_profile;changed.automatic=command.value;
                esp_err_t rc=profile_write(&changed);
                if(rc==ESP_OK){
                    s_profile=changed;
                    if(s_pending_save)s_attempt.automatic=changed.automatic;
                    profile_publish();
                }
                error_set(rc);memset(&changed,0,sizeof(changed));
            }break;
            case W_IDLE:if(!s_want_link)driver_close();break;
            default:break;
            }
            memset(&command,0,sizeof(command));
        }
        network_event_t event;
        while(xQueueReceive(s_events,&event,0)==pdTRUE)handle_event(&event);
        wifi_station_status_t status;wifi_station_status(&status);
        if(status.connecting && now_ms()-s_connect_at>=25000){driver_close();error_set(ESP_ERR_TIMEOUT);}
        if(status.online){wifi_ap_record_t ap;if(esp_wifi_sta_get_ap_info(&ap)==ESP_OK){portENTER_CRITICAL(&s_guard);s_status.rssi=ap.rssi;portEXIT_CRITICAL(&s_guard);}}
    }
}
void wifi_station_init(void){
    if(s_commands)return;
    s_commands=xQueueCreate(4,sizeof(command_t));s_events=xQueueCreate(12,sizeof(network_event_t));
    if(!s_commands || !s_events || xTaskCreate(worker,"wifi_station",6144,NULL,3,NULL)!=pdPASS){
        if(s_commands)vQueueDelete(s_commands);
        if(s_events)vQueueDelete(s_events);
        s_commands=s_events=NULL;
        error_set(ESP_ERR_NO_MEM);
    }
}
static bool command_send(uint8_t type,uint8_t value){
    command_t command={.type=type,.value=value};return s_commands && xQueueSend(s_commands,&command,0)==pdTRUE;
}
void wifi_station_status(wifi_station_status_t *out){portENTER_CRITICAL(&s_guard);*out=s_status;portEXIT_CRITICAL(&s_guard);}
bool wifi_station_ap(unsigned index,wifi_ap_record_t *out){
    portENTER_CRITICAL(&s_guard);bool ok=index<s_status.count;if(ok)*out=s_aps[index];portEXIT_CRITICAL(&s_guard);return ok;
}
bool wifi_station_scan(void){
    portENTER_CRITICAL(&s_guard);bool allowed=!s_status.scanning && !s_status.connecting;
    if(allowed){s_status.scanning=true;s_status.error=0;}portEXIT_CRITICAL(&s_guard);
    if(!allowed)return false;
    if(command_send(W_SCAN,0))return true;
    portENTER_CRITICAL(&s_guard);s_status.scanning=false;portEXIT_CRITICAL(&s_guard);return false;
}
bool wifi_station_supported(wifi_auth_mode_t auth){
    return auth==WIFI_AUTH_OPEN || auth==WIFI_AUTH_WPA_PSK || auth==WIFI_AUTH_WPA2_PSK ||
        auth==WIFI_AUTH_WPA_WPA2_PSK || auth==WIFI_AUTH_WPA2_WPA3_PSK;
}
bool wifi_station_connect(const wifi_ap_record_t *ap,const char *password){
    if(!ap || !password || !wifi_station_supported(ap->authmode) || !ap->ssid[0] || strlen(password)>64)return false;
    command_t command={.type=W_CONNECT};wifi_credentials_t *p=&command.credentials;
    p->version=1;p->security=ap->authmode==WIFI_AUTH_OPEN?WIFI_CRED_OPEN:WIFI_CRED_PSK;
    memcpy(p->ssid,ap->ssid,32);p->ssid[32]=0;
    snprintf(p->password,sizeof(p->password),"%s",password);
    wifi_station_status_t status;wifi_station_status(&status);p->automatic=status.automatic;
    bool ok=wifi_credentials_valid(p) && s_commands && xQueueSend(s_commands,&command,0)==pdTRUE;
    memset(&command,0,sizeof(command));return ok;
}
bool wifi_station_connect_saved(void){return command_send(W_SAVED,0);}
bool wifi_station_disconnect(void){return command_send(W_DISCONNECT,0);}
bool wifi_station_forget(void){return command_send(W_FORGET,0);}
bool wifi_station_auto(bool enabled){return command_send(W_AUTO,enabled);}
void wifi_station_idle(void){command_send(W_IDLE,0);}
