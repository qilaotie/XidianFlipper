#include "pet_service.h"
#include "nvs.h"
#include "esp_timer.h"
#include <string.h>
static pet_t state;
typedef struct {uint8_t version;char text[PET_NAME_MAX+1];} name_t;
static name_t pet_name;
static bool name_blocked;
static bool name_valid(const char *s){
    if(!s)return false;
    bool visible=false;unsigned n=0;
    for(;n<=PET_NAME_MAX && s[n];n++){
        if((unsigned char)s[n]<32 || (unsigned char)s[n]>126)return false;
        if(s[n]!=' ')visible=true;
    }
    return n<=PET_NAME_MAX && visible;
}
typedef struct {uint8_t version,views[9],reserved[2];uint32_t seconds,next_visit;} extra_t;
static extra_t extra;
_Static_assert(sizeof(extra_t)==20,"Pet extra layout");
static bool initialized, writable, dirty, pending, error;
static int64_t last_second,last_save,last_action=-2000000;
static unsigned reward_progress[PET_REWARD_COUNT];
static bool reward_trained[PET_REWARD_COUNT];
static void init(void){
    pet_name=(name_t){.version=1};name_blocked=false;
    pet_new(&state);extra=(extra_t){.version=1};writable=true;
    nvs_handle_t h;esp_err_t rc=nvs_open("pet_user",NVS_READONLY,&h);
    if(rc==ESP_OK){
        pet_t saved={0};size_t size=sizeof(saved);rc=nvs_get_blob(h,"state_v2",&saved,&size);
        if(rc==ESP_OK && size!=sizeof(saved)){writable=false;error=true;}
        if(rc==ESP_ERR_NVS_NOT_FOUND){
            pet_v1_t old={0};size_t old_size=sizeof(old);rc=nvs_get_blob(h,"state",&old,&old_size);
            if(rc==ESP_OK){
                if(old_size==sizeof(old) && pet_migrate(&saved,&old))dirty=pending=true;
                else {writable=false;error=true;}
            }
        }
        if(rc==ESP_OK && writable && pet_valid(&saved))state=saved;
        else if(rc!=ESP_ERR_NVS_NOT_FOUND){writable=false;error=true;}
        else dirty=pending=true;
        if(writable){
            extra_t saved_extra={0};size_t extra_size=sizeof(saved_extra);
            rc=nvs_get_blob(h,"extra_v1",&saved_extra,&extra_size);
            if(rc==ESP_OK){
                bool valid=extra_size==sizeof(extra) && saved_extra.version==1 && !saved_extra.reserved[0] && !saved_extra.reserved[1] &&
                    saved_extra.seconds<=0xffffff00u && saved_extra.next_visit<=saved_extra.seconds+180;
                for(unsigned i=0;i<9;i++)if(saved_extra.views[i]>3)valid=false;
                if(valid)extra=saved_extra;else {writable=false;error=true;}
            }else if(rc!=ESP_ERR_NVS_NOT_FOUND){writable=false;error=true;}
        }
        name_t saved_name={0};size_t name_size=sizeof(saved_name);
        esp_err_t name_rc=nvs_get_blob(h,"name_v1",&saved_name,&name_size);
        if(name_rc==ESP_OK && name_size==sizeof(saved_name) && saved_name.version==1 &&
           saved_name.text[PET_NAME_MAX]==0 && name_valid(saved_name.text))pet_name=saved_name;
        else if(name_rc!=ESP_ERR_NVS_NOT_FOUND)name_blocked=true;
        nvs_close(h);
    }else if(rc==ESP_ERR_NVS_NOT_FOUND)dirty=pending=true;
    else {writable=false;error=true;}
    initialized=true;last_second=last_save=esp_timer_get_time();
}
static bool save(void){
    if(!writable)return false;
    if(!dirty)return true;
    nvs_handle_t h;esp_err_t rc=nvs_open("pet_user",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"extra_v1",&extra,sizeof(extra));if(rc==ESP_OK)rc=nvs_set_blob(h,"state_v2",&state,sizeof(state));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    last_save=esp_timer_get_time();error=rc!=ESP_OK;
    if(!error)dirty=pending=false;
    return !error;
}
void pet_service_tick(void){
    if(!initialized)init();
    int64_t now=esp_timer_get_time();
    uint32_t seconds=(uint32_t)((now-last_second)/1000000);
    if(seconds && writable){
        pet_advance(&state,seconds);extra.seconds=seconds>0xffffff00u-extra.seconds?0xffffff00u:extra.seconds+seconds;
        dirty=true;last_second+=(int64_t)seconds*1000000;
    }
    if(dirty && now-last_save>=(pending?5000000:60000000))save();
}
const pet_t *pet_service_state(void){return &state;}
const char *pet_service_name(void){if(!initialized)init();return pet_name.text;}
bool pet_service_set_name(const char *name){
    if(!initialized)init();
    if(!writable || name_blocked || !name_valid(name))return false;
    name_t next={.version=1};memcpy(next.text,name,strlen(name)+1);
    nvs_handle_t h;esp_err_t rc=nvs_open("pet_user",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"name_v1",&next,sizeof(next));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    if(rc!=ESP_OK)return false;
    pet_name=next;return true;
}
static bool change(unsigned operation,unsigned argument){
    pet_service_tick();int64_t now=esp_timer_get_time();
    if(!writable || now-last_action<2000000)return false;
    bool ok=false;
    if(operation==0)ok=pet_act(&state,argument);
    if(operation==1)ok=pet_buy(&state,argument);
    if(operation==2)ok=pet_use(&state,argument);
    if(operation==3)ok=pet_game(&state,argument!=0);
    if(operation==4){pet_new_egg(&state);ok=true;}
    if(operation==5)ok=pet_room_choose(&state,argument);
    if(!ok)return false;
    last_action=now;dirty=pending=true;
    if(now-last_save>=5000000)save();
    return true;
}
bool pet_service_action(unsigned action){return change(0,action);}
bool pet_service_buy(unsigned item){return change(1,item);}
bool pet_service_use(unsigned item){return change(2,item);}
bool pet_service_game(bool won){return change(3,won);}
bool pet_service_complete_game(bool won){
    pet_service_tick();if(!writable)return false;
    if(won){pet_add_gold(&state,8);if(state.games_won<65535)state.games_won++;dirty=pending=true;}
    return true;
}
bool pet_service_new_egg(void){return change(4,0);}
bool pet_service_room(unsigned room){return change(5,room);}
static bool save_extra(void){
    nvs_handle_t h;esp_err_t rc=nvs_open("pet_user",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,"extra_v1",&extra,sizeof(extra));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    if(rc!=ESP_OK)error=true;
    return rc==ESP_OK;
}
bool pet_service_help_needed(unsigned item){pet_service_tick();return item<9 && extra.views[item]<3;}
void pet_service_help_seen(unsigned item){
    pet_service_tick();if(!writable || item>=9 || extra.views[item]>=3)return;
    extra.views[item]++;if(!save_extra())extra.views[item]--;
}
unsigned pet_service_social(unsigned outcome,unsigned seed,int *gift){
    pet_service_tick();*gift=-1;if(!writable || outcome>3)return 0;
    if(!outcome){
        if(extra.seconds<extra.next_visit)return 0;
        uint32_t previous=extra.next_visit;extra.next_visit=extra.seconds+180;
        /* Persist the gate BEFORE awarding. Power loss can lose a gift but cannot
         * duplicate it by rebooting between wallet and cooldown writes. */
        if(!save_extra()){extra.next_visit=previous;return 0;}
        unsigned item=seed%3;if(state.items[item]<9){state.items[item]++;*gift=(int)item;}
    }
    static const uint8_t amounts[]={5,6,10,20};
    unsigned credit=pet_add_gold(&state,amounts[outcome]);
    if(outcome==3 && state.games_won<65535)state.games_won++;
    dirty=pending=true;return credit;
}
void pet_service_reward_begin(unsigned source){
    if(source<PET_REWARD_COUNT){reward_progress[source]=0;reward_trained[source]=false;}
}
void pet_service_reward_restore(unsigned source,unsigned progress){
    if(source<PET_REWARD_COUNT){reward_progress[source]=progress;reward_trained[source]=progress>=(source==PET_REWARD_TETRIS?4u:5u);}
}
unsigned pet_service_reward(unsigned source,unsigned progress){
    pet_service_tick();
    if(!writable || source>=PET_REWARD_COUNT)return 0;
    unsigned limit=source==PET_REWARD_SNAKE?189:100000;
    if(progress>limit)progress=limit;
    if(progress<=reward_progress[source])return 0;
    unsigned amount=(progress-reward_progress[source])*(source==PET_REWARD_TETRIS?5:2);
    reward_progress[source]=progress; /* Advance even at gold cap: no deferred claims after spending. */
    unsigned credit=pet_add_gold(&state,amount);
    if(!reward_trained[source] && progress>=(source==PET_REWARD_TETRIS?4u:5u)){
        reward_trained[source]=true;
        if(pet_stage(&state) && !state.sleeping && state.games_won<65535){state.games_won++;dirty=pending=true;}
    }
    if(credit)dirty=pending=true;
    return credit;
}
bool pet_service_flush(void){pet_service_tick();return writable?save():!dirty;}
bool pet_service_error(void){return error;}
bool pet_service_pending(void){return pending;}
