#include "pet_page.h"
#include "pet_service.h"
#include "pet_games.h"
#include "pet_help.h"
#include "pet_link.h"
#include "wououi.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <stdio.h>
static unsigned selected;
enum {HOME,SHOP,BAG,DEX,NEW_EGG,GAME,GROWTH,GAMES,MINIGAME,CONNECT,LINK,HELP,GUIDES};
static bool help_link,help_manual;
static unsigned help_page,help_item,help_mask;
static int64_t help_at;
static pet_game_t mini;
static int64_t mini_at;
static bool mini_settled;
static unsigned mode,cursor;
static int64_t go_at;
static bool active,stats;
static int64_t effect_until;
static const char *notice;
static const char *const actions[]={"FEED","PLAY","CLEAN","SLEEP","SHOP","BAG","DEX","GROWTH","CALLS","CONNECT","HOW TO"};
static const char *const items[]={"APPLE","SNACK","SOAP","BALL","PLANT","STARS","RUG","NO DECOR"};
static const char *const species[]={"EGG","BABY","TEEN","SUNNY","SPORT","MELLOW"};
static void label(int x,int y,const char *s){u8g2_DrawStr(&u8g2,x,y,s);}
void pet_page_enter(uint8_t page){
    if(active && page!=M_PET){pet_link_leave();pet_service_flush();}
    active=page==M_PET;
    if(active){pet_service_tick();selected=mode=cursor=0;stats=false;effect_until=0;notice=NULL;}
}
bool pet_page_allows_attention(void){return mode!=GAME && mode!=MINIGAME && mode!=LINK;}
static void creature(const pet_t *p,unsigned frame){
    unsigned stage=pet_stage(p),phase=(frame/4)%8;
    int dx=0,bob=(int)((frame/2)%2),eye=0;
    bool wave=false,stretch=false,sit=false;
    if(!p->sleeping && p->energy>=25){
        if(phase==0)eye=-1;
        if(phase==1 || phase==5)dx=(int)(frame%5)-2;
        if(phase==2)bob=-(int)(frame%2);
        if(phase==3)stretch=true;
        if(phase==4)eye=1;
        if(phase==6)sit=true;
        if(phase==7)wave=true;
    }else if(!p->sleeping)sit=true;
    if(!stage){
        u8g2_DrawEllipse(&u8g2,19,19+bob,9,11,U8G2_DRAW_ALL);
        u8g2_DrawLine(&u8g2,14,16+bob,18,19+bob);u8g2_DrawLine(&u8g2,18,19+bob,23,15+bob);
        return;
    }
    int y=stage==1?12+bob:8+bob,h=stage==1?15:20;
    if(stretch)h++;
    if(sit){y++;h--;}
    u8g2_DrawRFrame(&u8g2,6+dx,y,27,h,5);
    if(stage==3 && p->form==PET_SPORT){
        u8g2_DrawRFrame(&u8g2,9+dx,y-6,4,8,1);u8g2_DrawRFrame(&u8g2,25+dx,y-6,4,8,1);
    }else if(stage!=3 || p->form!=PET_MELLOW){
        u8g2_DrawLine(&u8g2,8+dx,y+2,10+dx,y-4);u8g2_DrawLine(&u8g2,10+dx,y-4,15+dx,y);
        u8g2_DrawLine(&u8g2,25+dx,y,29+dx,y-4);u8g2_DrawLine(&u8g2,29+dx,y-4,31+dx,y+2);
    }else u8g2_DrawLine(&u8g2,17+dx,y,20+dx,y-3);
    if(p->sleeping || sit || frame%8==7){
        u8g2_DrawHLine(&u8g2,12+dx,y+6,4);u8g2_DrawHLine(&u8g2,24+dx,y+6,4);
    }else{
        u8g2_DrawBox(&u8g2,13+dx+eye,y+5,2,3);u8g2_DrawBox(&u8g2,25+dx+eye,y+5,2,3);
    }
    if(p->sleeping)label(34,10,"Z");
    if(p->mood<25)u8g2_DrawHLine(&u8g2,18+dx,y+11,5);
    else {u8g2_DrawLine(&u8g2,18+dx,y+10,20+dx,y+12);u8g2_DrawLine(&u8g2,20+dx,y+12,22+dx,y+10);}
    u8g2_DrawHLine(&u8g2,8+dx,y+h,5);u8g2_DrawHLine(&u8g2,25+dx,y+h,5);
    if(wave){u8g2_DrawLine(&u8g2,3,y+8,1,y+4);u8g2_DrawLine(&u8g2,35,y+8,38,y+4);}
    if(p->clean<25){u8g2_DrawPixel(&u8g2,3,27);u8g2_DrawPixel(&u8g2,36,24);}
}
static void feedback(bool ok,const char *yes,const char *no,int64_t now){
    notice=ok?yes:no;effect_until=now+1400000;
}
static void game_result(int64_t now){
    bool won=now>=go_at && now-go_at<=1200000;
    feedback(pet_service_game(won),won?"WIN +8G":(now<go_at?"TOO EARLY":"TOO SLOW"),"NEED REST / WAIT",now);
    mode=HOME;
}
static void decoration(const pet_t *p){
    if(p->decor==1){
        u8g2_DrawBox(&u8g2,1,27,5,4);u8g2_DrawVLine(&u8g2,3,20,7);
        u8g2_DrawLine(&u8g2,3,24,0,21);u8g2_DrawLine(&u8g2,3,23,6,20);
    }
    if(p->decor==2){
        u8g2_DrawLine(&u8g2,33,3,39,3);u8g2_DrawVLine(&u8g2,36,0,7);
        u8g2_DrawLine(&u8g2,0,7,6,7);u8g2_DrawVLine(&u8g2,3,4,7);
    }
    if(p->decor==3)u8g2_DrawRFrame(&u8g2,1,28,38,4,1);
}
static void game_visual(const pet_t *p,int64_t now){
    unsigned elapsed=(unsigned)((now-mini_at)/1000);
    if(mini.kind==PG_NOTES || mini.kind==PG_HEADING){
        unsigned left=mini.deadline>elapsed?mini.deadline-elapsed:0;
        int y=3+(int)((1300-(left>1300?1300:left))*20/1300);
        int x=6+12*mini.target;
        u8g2_DrawCircle(&u8g2,x,y,2,U8G2_DRAW_ALL);
        if(mini.kind==PG_NOTES){u8g2_DrawVLine(&u8g2,x+2,y-2<0?0:y-2,5);u8g2_DrawHLine(&u8g2,x+2,y-2<0?0:y-2,3);}
        u8g2_DrawRFrame(&u8g2,1+12*mini.lane,25,10,6,1);
    }else if(mini.kind==PG_BUMP){
        u8g2_DrawRFrame(&u8g2,1,10,14,15,3);u8g2_DrawRFrame(&u8g2,24,10,14,15,3);
        u8g2_DrawHLine(&u8g2,2,30,36);u8g2_DrawBox(&u8g2,2+(int)mini.meter*3,27,3,3);
    }else if(mini.kind==PG_SPRINT){
        int x=1+(int)(mini.score*22/40);u8g2_DrawRFrame(&u8g2,x,12+(int)((now/100000)%2),12,13,3);
        u8g2_DrawVLine(&u8g2,37,3,27);u8g2_DrawHLine(&u8g2,1,29,36);
    }else{
        creature(p,(unsigned)(now/400000));
        if(mini.kind==PG_FLAGS){
            if(mini.target!=1){u8g2_DrawVLine(&u8g2,1,2,12);u8g2_DrawBox(&u8g2,1,2,7,4);}
            if(mini.target!=0){u8g2_DrawVLine(&u8g2,38,2,12);u8g2_DrawBox(&u8g2,31,2,7,4);}
        }
    }
}
static void start_play(int64_t now){
    const pet_t *p=pet_service_state();
    if(cursor==PG_COUNT){
        if(pet_stage(p) && !p->sleeping && p->energy>=8 && p->fullness>=3){mode=GAME;go_at=now+2200000+(esp_random()%1800001);effect_until=0;}
        else feedback(false,"","WAIT / WAKE UP",now);
    }else if(pet_service_action(PET_PLAY)){
        pet_games_start(&mini,cursor,esp_random());mini_at=now;mini_settled=false;mode=MINIGAME;effect_until=0;
    }else feedback(false,"","WAIT / WAKE UP",now);
}
static void open_help(unsigned item,bool manual,int64_t now){
    help_item=item;help_link=item>=PET_HELP_VISIT;help_manual=manual;
    help_page=0;help_mask=0;help_at=now;mode=HELP;effect_until=0;
}
static void close_help(int64_t now){
    if(!help_manual && now-help_at>=2000000 && help_mask==((1u<<pet_help_pages(help_item))-1))
        pet_service_help_seen(help_item);
}
void pet_page_proc(void){
    pet_service_tick();int64_t now=esp_timer_get_time();
    if(btn.pressed){
        int key=btn.id;btn.pressed=false;
        if(key==BTN_ID_BACK){
            if(mode){
                if(mode==LINK){pet_link_leave();mode=CONNECT;}
                else if(mode==HELP){close_help(now);mode=help_manual?GUIDES:(help_link?CONNECT:GAMES);}
                else if(mode==MINIGAME){mode=GAMES;pet_service_flush();}
                else mode=HOME;
                effect_until=0;
            }
            else {pet_service_flush();if(ui.layer){ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}return;}
        }else if(mode==HELP){
            unsigned pages=pet_help_pages(help_item);
            if(key==BTN_ID_CW)help_page=(help_page+1)%pages;
            if(key==BTN_ID_CC)help_page=(help_page+pages-1)%pages;
            if(key==BTN_ID_SP){
                if(help_manual)mode=GUIDES;
                else if(help_link){pet_link_enter(cursor!=0);mode=LINK;}
                else start_play(now);
                if(mode!=HELP)close_help(now);
            }
        }else if(mode==MINIGAME){
            if(mini.done){if(key==BTN_ID_SP){mode=GAMES;pet_service_flush();}}
            else pet_games_step(&mini,(uint32_t)((now-mini_at)/1000),key==BTN_ID_CW?PG_RIGHT:(key==BTN_ID_CC?PG_LEFT:(key==BTN_ID_SP?PG_TAP:PG_NONE)));
        }else if(mode==LINK){
            pet_link_proc(key==BTN_ID_CW?PG_RIGHT:(key==BTN_ID_CC?PG_LEFT:(key==BTN_ID_SP?PG_TAP:PG_NONE)));return;
        }else if(mode==GAME){
            if(key==BTN_ID_SP)game_result(now);
        }else{
            if(key==BTN_ID_LP && mode==HOME)stats=!stats;
            unsigned count=mode==HOME?11:(mode==GUIDES?9:(mode==GAMES?7:(mode==SHOP?PET_ITEMS:(mode==BAG?PET_ITEMS+1:(mode==DEX?7:(mode==GROWTH?1:2))))));
            unsigned *position=mode==HOME?&selected:&cursor;
            if(key==BTN_ID_CW)*position=(*position+1)%count;
            if(key==BTN_ID_CC)*position=(*position+count-1)%count;
            if(key==BTN_ID_SP){
                if(mode==HOME){
                    if(selected==10){mode=GUIDES;cursor=0;effect_until=0;}
                    else if(selected==9){mode=CONNECT;cursor=0;effect_until=0;}
                    else if(selected==7){mode=GROWTH;cursor=0;effect_until=0;}
                    else if(selected==8){
                        bool ok=pet_service_action(PET_CALLS);
                        feedback(ok,pet_service_state()->spare?"CALLS OFF":"CALLS ON","WAIT",now);
                    }
                    else if(selected>=4){mode=selected-3;cursor=0;effect_until=0;}
                    else if(selected==PET_PLAY){
                        mode=GAMES;cursor=0;effect_until=0;
                    }else feedback(pet_service_action(selected),"HAPPY!","WAIT / WAKE UP",now);
                }else if(mode==GUIDES)open_help(cursor,true,now);
                else if(mode==GAMES || mode==CONNECT){
                    bool link=mode==CONNECT;unsigned item=link?PET_HELP_VISIT+cursor:cursor;
                    if(pet_service_help_needed(item))open_help(item,false,now);
                    else if(link){pet_link_enter(cursor!=0);mode=LINK;}
                    else start_play(now);
                }
                else if(mode==SHOP)feedback(pet_service_buy(cursor),"BOUGHT!","NO GOLD / OWNED",now);
                else if(mode==BAG)feedback(pet_service_use(cursor),"USED / EQUIPPED","EMPTY / NOT NOW",now);
                else if(mode==DEX && cursor==6){mode=NEW_EGG;cursor=0;}
                else if(mode==NEW_EGG){
                    if(!cursor){mode=DEX;cursor=6;}
                    else if(pet_service_new_egg()){mode=HOME;selected=0;feedback(true,"NEW EGG!","",now);}
                    else feedback(false,"","WAIT / SAVE ERR",now);
                }
            }
        }
    }
    if(mode==GAME && now>go_at+3000000)game_result(now);
    if(mode==LINK){pet_link_proc(PG_NONE);return;}
    if(mode==HELP){
        unsigned item=help_item;help_mask|=1u<<help_page;
        u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
        for(unsigned row=0;row<3;row++)label(1,7+8*(int)row,pet_help_line(item,help_page,row));
        char footer[27];snprintf(footer,sizeof(footer),"%u/%u TURN:PAGE SP:%s",help_page+1,pet_help_pages(item),help_manual?"BACK":"START");
        label(1,31,now<effect_until && notice?notice:footer);return;
    }
    if(mode==MINIGAME){
        pet_games_step(&mini,(uint32_t)((now-mini_at)/1000),PG_NONE);
        if(mini.done && !mini_settled){mini_settled=true;pet_service_complete_game(mini.won);pet_service_flush();}
    }
    const pet_t *p=pet_service_state();unsigned stage=pet_stage(p);
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);
    if(mode==DEX && cursor<6){
        if(p->dex&(1u<<cursor)){
            pet_t preview=*p;preview.age=cursor==0?0:(cursor==1?60:(cursor==2?600:1800));
            preview.form=cursor>=3?(uint8_t)(cursor-2):0;preview.sleeping=0;preview.mood=preview.clean=100;
            creature(&preview,(unsigned)(now/400000));
        }else label(17,20,"?");
    }else if(mode==MINIGAME)game_visual(p,now);
    else {decoration(p);creature(p,(unsigned)(now/400000));}
    char line[20];
    if(mode==GAMES || mode==CONNECT || mode==GUIDES){
        label(42,7,mode==GAMES?"PET GAMES":(mode==GUIDES?"HOW TO PLAY":"CONNECTION"));
        label(42,15,mode==GUIDES?(cursor<PG_COUNT?pet_games_name(cursor):(cursor==PG_COUNT?"QUICK TAP":(cursor==PET_HELP_VISIT?"VISIT":"DUEL"))):
            (mode==GAMES?(cursor<PG_COUNT?pet_games_name(cursor):"QUICK TAP"):(cursor?"DUEL":"VISIT")));
        label(42,23,mode==GAMES?"WIN: +8G":(mode==GUIDES?"READ ONLY":"BOTH SAME MODE"));
    }else if(mode==MINIGAME){
        snprintf(line,sizeof(line),"%s %u",pet_games_name(mini.kind),mini.score);label(42,7,line);
        if(mini.done){label(42,15,mini.won?"WIN +8G":"TRY AGAIN");label(42,23,"SP: GAME MENU");}
        else if(mini.kind==PG_MEMORY){
            static const char *const symbols[]={"?","LEFT","RIGHT","TAP"};
            snprintf(line,sizeof(line),"%s %s",mini.phase?"YOUR TURN":"REMEMBER",mini.phase?"":(mini.deadline-(uint32_t)((now-mini_at)/1000)<150?"-":symbols[mini.target]));label(42,15,line);
            snprintf(line,sizeof(line),"STEP %u/%u",mini.index+1,mini.length);label(42,23,line);
        }else if(mini.kind==PG_BUMP){snprintf(line,sizeof(line),"POWER %u",mini.meter);label(42,15,line);label(42,23,"SP: POWER>=7");}
        else if(mini.kind==PG_FLAGS){static const char *const flags[]={"LEFT","RIGHT","BOTH: TAP"};label(42,15,flags[mini.target]);label(42,23,"MATCH THE FLAG");}
        else if(mini.kind==PG_SPRINT){label(42,15,"TAP FAST!");snprintf(line,sizeof(line),"20s GOAL:30 %lus",(unsigned long)((now-mini_at)/1000000));label(42,23,line);}
        else{
            snprintf(line,sizeof(line),"YOU %u TARGET %u",mini.lane+1,mini.target+1);label(42,15,line);
            int64_t remaining=(int64_t)mini.deadline-(now-mini_at)/1000;
            snprintf(line,sizeof(line),"%s %ldms",mini.kind==PG_HEADING?"SP:LAST":"LAND",(long)(remaining>0?remaining:0));label(42,23,line);
        }
    }else if(mode==HOME){
        if(stage && now<effect_until && notice && notice[0]=='H'){
            if(selected==PET_FEED)u8g2_DrawDisc(&u8g2,36,19+(int)((now/200000)%2),2,U8G2_DRAW_ALL);
            if(selected==PET_CLEAN){u8g2_DrawCircle(&u8g2,3,8,2,U8G2_DRAW_ALL);u8g2_DrawCircle(&u8g2,36,13,2,U8G2_DRAW_ALL);}
        }
        if(stats){
            snprintf(line,sizeof(line),"FOOD %u MOOD %u",p->fullness,p->mood);label(42,7,line);
            snprintf(line,sizeof(line),"REST %u WASH %u",p->energy,p->clean);label(42,15,line);
        }else{
            const char *name=stage==3?species[p->form+2]:species[stage];
            snprintf(line,sizeof(line),"%s %lum",name,(unsigned long)(p->age/60));label(42,7,line);
            if(!stage)snprintf(line,sizeof(line),"HATCH IN %lus",(unsigned long)(60-p->age));
            else snprintf(line,sizeof(line),"%s",p->sleeping?"SLEEPING":(p->fullness<20?"HUNGRY":(p->energy<20?"TIRED":(p->clean<20?"NEED WASH":"FEELING GOOD"))));
            label(42,15,line);
        }
        const char *action=selected==PET_SLEEP && p->sleeping?"WAKE":(selected==8?(p->spare?"CALLS OFF":"CALLS ON"):actions[selected]);
        snprintf(line,sizeof(line),">%s %uG",action,p->coins);label(42,23,line);
    }else if(mode==SHOP || mode==BAG){
        snprintf(line,sizeof(line),"%s %uG",mode==SHOP?"SHOP":"BAG",p->coins);label(42,7,line);
        snprintf(line,sizeof(line),">%s",items[cursor]);label(42,15,line);
        if(cursor<3){
            if(mode==SHOP)snprintf(line,sizeof(line),"%uG  x%u",pet_price(cursor),p->items[cursor]);
            else snprintf(line,sizeof(line),"x%u %s",p->items[cursor],cursor==PET_APPLE?"FOOD +35":(cursor==PET_SNACK?"MOOD +25":"WASH MAX"));
        }
        else if(cursor==PET_ITEMS)snprintf(line,sizeof(line),"REMOVE DECOR");
        else if(mode==BAG)snprintf(line,sizeof(line),"%s",(p->owned&(1u<<(cursor-3)))?(cursor==PET_BALL?"MOOD+12 REST-4":"SP: EQUIP"):"BUY IN SHOP");
        else snprintf(line,sizeof(line),"%uG %s",pet_price(cursor),(p->owned&(1u<<(cursor-3)))?"OWNED":"NOT OWNED");
        label(42,23,line);
    }else if(mode==DEX){
        unsigned count=0;for(unsigned i=0;i<6;i++)if(p->dex&(1u<<i))count++;
        snprintf(line,sizeof(line),"DEX %u/6",count);label(42,7,line);
        label(42,15,cursor==6?"NEW EGG?":species[cursor]);
        label(42,23,cursor==6?"SP: CONFIRM":((p->dex&(1u<<cursor))?"DISCOVERED":"LOCKED"));
    }else if(mode==NEW_EGG){
        label(42,7,"RETIRE THIS PET?");
        label(42,15,cursor?">NEW EGG":">KEEP PET");
        label(42,23,"KEEP GOLD / DEX");
    }else if(mode==GROWTH){
        snprintf(line,sizeof(line),"%s %lum",p->form?"GROWN":"GROWTH",(unsigned long)(p->age/60));label(42,7,line);
        snprintf(line,sizeof(line),"CARE %u/24",p->care_good>24?24:p->care_good);label(42,15,line);
        snprintf(line,sizeof(line),"WINS %u/5",p->games_won>5?5:p->games_won);label(42,23,line);
    }else{
        label(42,7,now<go_at?"WAIT...":"GO! TAP");
        label(42,15,"TAP WITHIN 1.2s");label(42,23,"WIN: +8 GOLD");
    }
    label(42,31,pet_service_error()?"SAVE ERROR":(now<effect_until && notice?notice:
        (pet_service_pending()?"SAVING...":(mode==HOME?"HOLD:STATS":(mode==GAMES || mode==CONNECT?"SP:PLAY HELP:HOME":(mode==GUIDES?"SP: READ HELP":(mode==SHOP?"SP:BUY BACK:HOME":(mode==BAG?"SP:USE BACK:HOME":(mode==GROWTH?species[pet_growth_form(p)+2]:"BACK:RETURN")))))))));
}
