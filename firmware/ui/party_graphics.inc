/* Direct manipulation of the production snapshot; no optimistic game edits. */
static unsigned graphic_pick,graphic_color,graphic_category;
static bool graphic_wild,graphic_scores,graphic_reveal;
static void graphic_reset(void){graphic_pick=graphic_color=graphic_category=0;graphic_wild=graphic_scores=graphic_reveal=false;}
static void graphic_hide(void){graphic_reveal=false;graphic_wild=false;graphic_scores=false;}
static bool graphic_turn(void){return v.active && v.enabled && v.ready && !v.stale && !v.pending && v.game.me<v.game.n && v.game.me==v.game.turn && v.game.phase==PG_PLAYING;}
static void graphic_text(int x,int y,const char *s){u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);u8g2_DrawStr(&u8g2,x,y,s);}
static void graphic_frame(int x,int y,int w,int h){u8g2_DrawHLine(&u8g2,x,y,w);u8g2_DrawHLine(&u8g2,x,y+h-1,w);u8g2_DrawVLine(&u8g2,x,y,h);u8g2_DrawVLine(&u8g2,x+w-1,y,h);}
/* Four distinct monochrome suits replace colors on this one-bit screen. */
static void graphic_suit(int x,int y,unsigned suit){
    if(suit==0)graphic_frame(x,y,5,5);
    else if(suit==1){u8g2_DrawHLine(&u8g2,x,y+2,5);u8g2_DrawVLine(&u8g2,x+2,y,5);}
    else if(suit==2){u8g2_DrawBox(&u8g2,x+1,y,3,1);u8g2_DrawBox(&u8g2,x,y+1,5,3);u8g2_DrawBox(&u8g2,x+1,y+4,3,1);}
    else {u8g2_DrawBox(&u8g2,x+2,y,1,1);u8g2_DrawBox(&u8g2,x+1,y+1,3,2);u8g2_DrawHLine(&u8g2,x,y+3,5);}
}
static void graphic_card(int x,unsigned card,bool hidden,bool selected){
    graphic_frame(x,9,17,15);
    if(selected)u8g2_DrawHLine(&u8g2,x,24,17);
    if(hidden){for(int i=2;i<15;i+=3)u8g2_DrawVLine(&u8g2,x+i,11,11);return;}
    if(card<52)graphic_suit(x+2,11,card/13);
    else {graphic_suit(x+2,11,0);graphic_suit(x+9,11,1);}
    char text[4];unsigned rank=card%13;
    if(card>=52)snprintf(text,sizeof(text),"%s",card==52?"W":"+4");
    else if(rank<10)snprintf(text,sizeof(text),"%u",rank);
    else snprintf(text,sizeof(text),"%s",rank==10?"/":rank==11?"<>":"+2");
    graphic_text(x+3,22,text);
}
static void graphic_status(void){
    const char *s;
    if(esp_timer_get_time()<error_until && v.result!=TEAM_OK)s=error_text;
    else if(v.game.phase==PG_FINISHED)s=v.game.me<8 && (v.game.winner&(1u<<v.game.me))?T("YOU WIN  BACK:ROOM","你赢了 返回房间"):T("FINISHED  BACK:ROOM","本局结束 返回房间");
    else if(!v.enabled)s=T("NETWORK OFF","网络关闭");
    else if(v.stale)s=T("HOST OFFLINE","主机离线");
    else if(v.pending || !v.ready)s=T("SYNCING...","同步中 请稍等");
    else if(v.watching)s=T("Watching public table","观战公开桌面");
    else if(v.game.me!=v.game.turn)s=T("WAIT FOR OPPONENT","等待对方操作");
    else s=T("TURN:PICK TAP:USE","旋转选择 短按操作");
    (void)s;
    const char *status=esp_timer_get_time()<error_until && v.result!=TEAM_OK?"REJECTED / RETRY":v.game.phase==PG_FINISHED?(v.game.me<8 && (v.game.winner&(1u<<v.game.me))?"YOU WIN  BACK:ROOM":"FINISHED  BACK:ROOM"):!v.enabled?"NETWORK OFF":v.stale?"HOST OFFLINE":v.pending || !v.ready?"SYNCING...":v.watching?"WATCH PUBLIC TABLE":v.game.me!=v.game.turn?"WAIT FOR OPPONENT":"TURN:PICK TAP:USE";
    graphic_text(0,31,status);
}
static void graphic_header(void){char s[40];unsigned p=v.game.turn;if(v.game.kind==PG_UNO)snprintf(s,sizeof(s),"P%u %.6s %u/%u%s",p<v.game.n?p+1:0,p<v.game.n?v.names[p]:"",graphic_pick<v.game.nh?graphic_pick+1:0,v.game.nh,v.game.me==p?"*":"");else snprintf(s,sizeof(s),"P%u %.6s%s",p<v.game.n?p+1:0,p<v.game.n?v.names[p]:"",v.game.me==p?"*":"");graphic_text(0,7,s);}
static void graphic_uno(void){
    unsigned nh=v.watching?0:v.game.nh<=32?v.game.nh:32,total=v.watching?(v.game.n?v.game.n:1):nh+4;graphic_pick%=total;
    if(btn.pressed){unsigned key=btn.id;
        if(key==BTN_ID_CW || key==BTN_ID_CC){btn.pressed=false;int step=key==BTN_ID_CW?1:-1;if(graphic_wild)graphic_color=(graphic_color+4+step)%4;else graphic_pick=(graphic_pick+total+step)%total;}
        if(key==BTN_ID_LP){btn.pressed=false;graphic_reveal=!graphic_reveal;}
        if(key==BTN_ID_BACK && graphic_wild){btn.pressed=false;graphic_wild=false;}
        if(key==BTN_ID_SP){btn.pressed=false;
            if(graphic_wild){if(graphic_turn() && graphic_pick<nh && request(PARTY_MOVE,PG_CARD,graphic_pick,graphic_color,0))graphic_wild=false;}
            else if(graphic_pick<nh){if(graphic_turn() && graphic_reveal){if(v.game.hand[graphic_pick]>=52){graphic_color=v.game.data[0]%4;graphic_wild=true;}else request(PARTY_MOVE,PG_CARD,graphic_pick,0,0);}}
            else if(v.enabled && v.ready && !v.pending && !v.stale && v.game.me<v.game.n && v.game.phase==PG_PLAYING){unsigned a=graphic_pick-nh;
                if(a==0 && graphic_turn())request(PARTY_MOVE,PG_DRAW,0,0,0);
                if(a==1 && graphic_turn())request(PARTY_MOVE,PG_PASS,0,0,0);
                if(a==2)request(PARTY_MOVE,PG_UNO_CALL,0,0,0);
                if(a==3)for(unsigned p=0;p<v.game.n;p++)if(p!=v.game.me && v.game.board[p]==1 && !(v.game.flags[p]&PG_DECLARED)){request(PARTY_MOVE,PG_UNO_CATCH,p,0,0);break;}
            }
        }
    }
    graphic_header();graphic_suit(119,1,v.game.data[0]%4);
    graphic_card(0,v.game.last,false,false);
    if(v.watching){unsigned first=(graphic_pick/4)*4;for(unsigned i=0;i<4 && first+i<v.game.n;i++){int x=23+i*24;graphic_frame(x,9,21,15);char count[8];snprintf(count,sizeof(count),"P%u",first+i+1);graphic_text(x+2,16,count);snprintf(count,sizeof(count),"%u",v.game.board[first+i]);graphic_text(x+3,23,count);}graphic_status();return;}
    if(graphic_wild){for(unsigned i=0;i<4;i++){int x=26+i*24;graphic_frame(x,10,19,14);graphic_suit(x+7,15,i);if(i==graphic_color)u8g2_DrawHLine(&u8g2,x,24,19);}graphic_text(0,31,"PICK SUIT / TAP TO PLAY");return;}
    unsigned first=graphic_pick<nh?(graphic_pick/4)*4:nh>4?((nh-1)/4)*4:0;
    for(unsigned i=0;i<4 && first+i<nh;i++)graphic_card(23+i*19,v.game.hand[first+i],!graphic_reveal,first+i==graphic_pick);
    graphic_frame(102,9,26,15);const char *actions[]={"+","->","UNO","!"};graphic_text(106,20,graphic_pick<nh?"...":actions[graphic_pick-nh]);
    if(graphic_pick>=nh)u8g2_DrawHLine(&u8g2,102,24,26);
    if(!graphic_reveal && graphic_pick<nh && graphic_turn())graphic_text(0,31,"HOLD: SHOW / HIDE HAND");else graphic_status();
}
static void graphic_die(int x,unsigned value,bool kept,bool selected){
    graphic_frame(x,9,17,15);if(selected)u8g2_DrawHLine(&u8g2,x,24,17);
    if(kept)u8g2_DrawHLine(&u8g2,x+3,8,11);
    if(!value){graphic_text(x+6,21,"?");return;}
    if(value&1)u8g2_DrawBox(&u8g2,x+8,17,2,2);
    if(value>=2){u8g2_DrawBox(&u8g2,x+4,12,2,2);u8g2_DrawBox(&u8g2,x+12,20,2,2);}
    if(value>=4){u8g2_DrawBox(&u8g2,x+12,12,2,2);u8g2_DrawBox(&u8g2,x+4,20,2,2);}
    if(value==6){u8g2_DrawBox(&u8g2,x+4,17,2,2);u8g2_DrawBox(&u8g2,x+12,17,2,2);}
}
static void graphic_yacht(void){
    graphic_pick%=7;graphic_category%=13;
    if(btn.pressed){unsigned key=btn.id;
        if(key==BTN_ID_CW || key==BTN_ID_CC){btn.pressed=false;int step=key==BTN_ID_CW?1:-1;if(graphic_scores)graphic_category=(graphic_category+13+step)%13;else graphic_pick=(graphic_pick+7+step)%7;}
        if(key==BTN_ID_BACK && graphic_scores){btn.pressed=false;graphic_scores=false;}
        if(key==BTN_ID_SP){btn.pressed=false;if(graphic_turn()){
            if(graphic_scores){if(v.game.extra[graphic_category]==255 && request(PARTY_MOVE,PG_SCORE,graphic_category,0,0)){graphic_scores=false;graphic_pick=5;}}
            else if(graphic_pick<5)request(PARTY_MOVE,PG_KEEP,graphic_pick,0,0);
            else if(graphic_pick==5)request(PARTY_MOVE,PG_ROLL,0,0,0);
            else {graphic_scores=true;graphic_category=0;}
        }}
    }
    graphic_header();char s[40];snprintf(s,sizeof(s),"%u/3",v.game.data[0]);graphic_text(107,7,s);
    if(graphic_scores){
        ui_zh_draw_fit(&u8g2,0,20,category(graphic_category),87);
        unsigned score=party_yacht_score(v.game.board,graphic_category);bool used=v.game.extra[graphic_category]!=255;
        snprintf(s,sizeof(s),"%s%u",used?"X":"+",used?v.game.extra[graphic_category]:score);graphic_text(94,20,s);
        for(unsigned i=0;i<13;i++)if(v.game.extra[i]!=255)u8g2_DrawBox(&u8g2,i*9,21,7,2);else graphic_frame(i*9,21,7,2);
        u8g2_DrawBox(&u8g2,graphic_category*9,24,7,1);
        graphic_text(0,31,used?"USED / TURN TO PICK":"TAP:SCORE BACK:CANCEL");return;
    }
    for(unsigned i=0;i<5;i++)graphic_die(i*19,v.game.board[i],v.game.data[1]&(1u<<i),graphic_pick==i);
    if(graphic_pick==5){u8g2_DrawBox(&u8g2,98,8,30,8);u8g2_SetDrawColor(&u8g2,0);}graphic_text(102,15,"ROLL");u8g2_SetDrawColor(&u8g2,1);
    if(graphic_pick==6){u8g2_DrawBox(&u8g2,98,17,30,8);u8g2_SetDrawColor(&u8g2,0);}graphic_text(102,24,"SUM");u8g2_SetDrawColor(&u8g2,1);
    graphic_status();
}
static void graphic_game(void){
    if(!v.active){line(20,T("No room","没有房间"));return;}
    if(v.game.kind==PG_TIC){board();return;}
    if(v.game.phase==PG_FINISHED){graphic_wild=false;graphic_scores=false;}
    if(!v.ready || (v.game.me>=v.game.n && !v.watching)){line(20,T("Join / synchronizing","请加入或等待同步"));return;}
    if(v.game.kind==PG_UNO)graphic_uno();else if(v.game.kind==PG_YACHT)graphic_yacht();
}
