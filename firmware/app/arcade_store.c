#include "arcade_store.h"
#include "snake.h"
#include "tetris.h"
#include "pet_service.h"
#include "nvs.h"
#include <stddef.h>
#include <string.h>
typedef struct {
    uint32_t version,kind,best,best_progress,claimed,earned;
    union {snake_t snake;tetris_t tetris;} game;
    uint32_t checksum;
} record_t;
static record_t records[2];
static bool loaded[2],dirty[2],errors[2],blocked[2];
static uint32_t last_save[2];
static const char *const keys[]={"tetris_v1","snake_v1"};
static size_t game_size(unsigned k){return k==ARCADE_SNAKE?sizeof(snake_t):sizeof(tetris_t);}
static uint32_t hash(const record_t *r){
    uint32_t h=2166136261u;const unsigned char *p=(const unsigned char *)r;
    for(size_t i=0;i<offsetof(record_t,checksum);i++)h=(h^p[i])*16777619u;
    return h;
}
static unsigned progress(const record_t *r){return r->kind==ARCADE_SNAKE?(unsigned)r->game.snake.length-3u:r->game.tetris.lines;}
static unsigned mark(const record_t *r){unsigned p=progress(r),max=r->kind==ARCADE_SNAKE?189:100000;return p>max?max:p;}
static bool over(const record_t *r){return r->kind==ARCADE_SNAKE?r->game.snake.over:r->game.tetris.over;}
static bool valid(const record_t *r,unsigned k){
    if(r->version!=1 || r->kind!=k || r->checksum!=hash(r))return false;
    if(k==ARCADE_SNAKE){
        const snake_t *s=&r->game.snake;
        if(s->map>=SNAKE_MAPS || s->length<3 || s->length>snake_capacity(s->map) || s->direction>3 || s->turn<-1 || s->turn>1 || !s->rng ||
           ((const unsigned char *)s)[offsetof(snake_t,over)]>1 || ((const unsigned char *)s)[offsetof(snake_t,won)]>1)return false;
        if(s->won && (!s->over || s->length!=snake_capacity(s->map)))return false;
        for(unsigned i=0;i<s->length;i++){
            if(s->body[i]>=SNAKE_CELLS || snake_wall(s->map,s->body[i]))return false;
            for(unsigned j=0;j<i;j++)if(s->body[j]==s->body[i])return false;
            if(i){int dx=s->body[i]%24-s->body[i-1]%24,dy=s->body[i]/24-s->body[i-1]/24;
                if((dx<0?-dx:dx)+(dy<0?-dy:dy)!=1)return false;}
        }
        if(!s->won){if(s->food>=SNAKE_CELLS || snake_wall(s->map,s->food))return false;for(unsigned i=0;i<s->length;i++)if(s->body[i]==s->food)return false;}
        if(r->best>189 || r->best_progress>189 || r->earned>378)return false;
    }else{
        const tetris_t *g=&r->game.tetris;unsigned bag=0;
        if(g->piece>6 || g->next>6 || g->rotation>3 || g->remaining>7 || !g->rng ||
           g->x<-3 || g->x>9 || g->y<0 || g->y>19 || g->lines>1000000 ||
           ((const unsigned char *)g)[offsetof(tetris_t,over)]>1 || g->clear_mask)return false;
        for(unsigned i=0;i<7;i++){if(g->bag[i]>6 || (bag&(1u<<g->bag[i])))return false;bag|=1u<<g->bag[i];}
        for(unsigned i=0;i<20;i++)if(g->rows[i]&~0x3ffu)return false;
        if(!g->over && !tetris_fits(g,g->x,g->y,g->rotation))return false;
        if(r->best_progress>1000000 || r->earned>500000)return false;
    }
    return r->claimed<=mark(r) && r->best_progress>=progress(r);
}
static void capture(record_t *r,const void *g){
    memcpy(&r->game,g,game_size(r->kind));
    if(r->kind==ARCADE_TETRIS){r->game.tetris.clear_mask=0;memset(r->game.tetris.clear_rows,0,sizeof(r->game.tetris.clear_rows));}
    unsigned p=progress(r);uint32_t score=r->kind==ARCADE_SNAKE?p:r->game.tetris.score;
    if(score>r->best)r->best=score;
    if(p>r->best_progress)r->best_progress=p;
}
static bool save(unsigned k){
    if(blocked[k])return false;
    record_t *r=&records[k];r->checksum=hash(r);
    nvs_handle_t h;esp_err_t rc=nvs_open("arcade_user",NVS_READWRITE,&h);
    if(rc==ESP_OK){rc=nvs_set_blob(h,keys[k],r,sizeof(*r));if(rc==ESP_OK)rc=nvs_commit(h);nvs_close(h);}
    errors[k]=rc!=ESP_OK;if(!errors[k])dirty[k]=false;return !errors[k];
}
int arcade_load(unsigned k,void *g){
    if(k>=2)return -1;
    records[k]=(record_t){.version=1,.kind=k};dirty[k]=errors[k]=blocked[k]=false;loaded[k]=true;
    record_t saved={0};size_t size=sizeof(saved);nvs_handle_t h;esp_err_t rc=nvs_open("arcade_user",NVS_READONLY,&h);
    if(rc==ESP_OK){rc=nvs_get_blob(h,keys[k],&saved,&size);nvs_close(h);}
    if(rc==ESP_ERR_NVS_NOT_FOUND)return 0;
    if(rc!=ESP_OK || size!=sizeof(saved) || !valid(&saved,k)){errors[k]=blocked[k]=true;return -1;}
    records[k]=saved;memcpy(g,&saved.game,game_size(k));
    pet_service_reward_restore(k,saved.claimed);
    return over(&saved)?0:1;
}
bool arcade_new(unsigned k,const void *g){
    if(k>=2)return false;
    uint32_t best=records[k].best,lines=records[k].best_progress;
    records[k]=(record_t){.version=1,.kind=k,.best=best,.best_progress=lines};
    loaded[k]=true;blocked[k]=false;dirty[k]=true;capture(&records[k],g);
    pet_service_reward_restore(k,0);return save(k);
}
unsigned arcade_tick(unsigned k,const void *g,uint32_t now){
    if(k>=2 || !loaded[k] || blocked[k])return 0;
    record_t *r=&records[k],before=*r;capture(r,g);
    if(memcmp(&before,r,sizeof(*r)))dirty[k]=true;
    unsigned p=mark(r);
    if(p>r->claimed && !pet_service_error()){
        unsigned old=r->claimed,old_earned=r->earned;
        pet_service_tick();unsigned amount=(p-old)*(k==ARCADE_SNAKE?2:5),room=9999-pet_service_state()->coins;
        if(amount>room)amount=room;
        r->claimed=p;r->earned+=amount;dirty[k]=true;
        if(save(k)){
            pet_service_reward_restore(k,old);pet_service_reward(k,p);pet_service_flush();last_save[k]=now;
        }else {r->claimed=old;r->earned=old_earned;}
    }
    if(dirty[k] && (over(r)!=over(&before) || now-last_save[k]>=30000)){
        save(k);last_save[k]=now;
    }
    return r->earned;
}
bool arcade_flush(void){bool ok=true;for(unsigned k=0;k<2;k++)if(loaded[k] && dirty[k]){if(!save(k))ok=false;}return ok;}
bool arcade_error(unsigned k){return k<2 && errors[k];}
uint32_t arcade_best(unsigned k,bool p){return k<2?(p?records[k].best_progress:records[k].best):0;}
bool arcade_record_best(unsigned k,uint32_t *score,uint32_t *p){
    if(k>=2 || !score || !p)return false;
    record_t saved={0};const record_t *r=&saved;
    if(loaded[k]){if(blocked[k])return false;r=&records[k];}
    else {nvs_handle_t h;size_t size=sizeof(saved);esp_err_t rc=nvs_open("arcade_user",NVS_READONLY,&h);
        if(rc==ESP_OK){rc=nvs_get_blob(h,keys[k],&saved,&size);nvs_close(h);}
        if(rc!=ESP_OK || size!=sizeof(saved) || !valid(&saved,k))return false;}
    *score=r->best;*p=k==ARCADE_SNAKE?r->best+3:r->best_progress;return true;
}
