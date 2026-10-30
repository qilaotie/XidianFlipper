/* Small-screen views for restored card and social-deduction games. */
/* Local selection only. Every committed move still goes through the host. */
static unsigned adapt_stage,adapt_card,adapt_target,adapt_guess,adapt_confirm;
static uint32_t adapt_revision;
static void adapt_reset(void){adapt_stage=adapt_card=adapt_target=adapt_confirm=0;adapt_guess=2;}
static void adapt_turn(unsigned count){
    if(!count){cell=0;return;}cell%=count;
    if(btn.pressed && (btn.id==BTN_ID_CW || btn.id==BTN_ID_CC)){int step=btn.id==BTN_ID_CW?1:-1;cell=(cell+count+step)%count;btn.pressed=false;}
}
static bool adapt_ready(void){return v.active && v.enabled && v.ready && !v.pending && !v.stale && v.game.me<v.game.n && v.game.phase!=PG_LOBBY && v.game.phase!=PG_FINISHED;}
static void adapt_open(unsigned a,unsigned b){initial_a=a;initial_b=b;initial_a_set=true;ui_navigate_to(M_PARTY_ACTION);}
static bool adapt_commit(unsigned action,unsigned a,unsigned b,unsigned c){
    if(!adapt_ready() || v.game.me!=v.game.turn)return false;
    if(!request(PARTY_MOVE,action,a,b,c))return false;
    adapt_reset();return true;
}
static void adapt_begin(unsigned card,unsigned target,unsigned stage){adapt_card=card;adapt_target=target;adapt_stage=stage;adapt_guess=v.game.kind==PG_CODE?0:2;adapt_confirm=0;}
static void adapt_status(void){
    if(!adapt_ready() || v.game.me!=v.game.turn || (esp_timer_get_time()<error_until && v.result!=TEAM_OK)){
        u8g2_SetDrawColor(&u8g2,0);u8g2_DrawBox(&u8g2,0,25,128,7);u8g2_SetDrawColor(&u8g2,1);graphic_status();
    }
}
static bool adapt_target_ok(unsigned p){
    unsigned card=v.game.hand[adapt_card];
    return p<v.game.n && (v.game.flags[p]&PG_ALIVE) && !(v.game.flags[p]&PG_PROTECTED) && (p!=v.game.me || card==5);
}
/* Inline target / number / explicit yes-no picker; BACK never submits. */
static bool adapt_picker(void){
    if(!adapt_stage)return false;
    if(!adapt_ready() || v.game.me!=v.game.turn){adapt_reset();graphic_status();return true;}
    unsigned key=btn.pressed?btn.id:0;
    if(key==BTN_ID_BACK){btn.pressed=false;adapt_reset();return true;}
    if(key==BTN_ID_CW || key==BTN_ID_CC){
        int step=key==BTN_ID_CW?1:-1;btn.pressed=false;
        if(adapt_stage==1){for(unsigned i=0;i<v.game.n;i++){adapt_target=(adapt_target+v.game.n+step)%v.game.n;if(adapt_target_ok(adapt_target))break;}}
        else if(adapt_stage==2){unsigned min=v.game.kind==PG_CODE?0:2,count=v.game.kind==PG_CODE?12:7;adapt_guess=min+(adapt_guess-min+count+step)%count;}
        else adapt_confirm^=1;
    }
    char s[48];
    if(v.game.kind==PG_CODE)snprintf(s,sizeof(s),"P%u TILE %u",adapt_card+1,adapt_target+1);
    else if(v.game.kind==PG_LOVE)card_name(PG_LOVE,v.game.hand[adapt_card],s);
    else snprintf(s,sizeof(s),"R%u MASK %02X",v.game.extra[14],liar_mask);
    line(11,s);
    if(adapt_stage==1){
        for(unsigned p=0;p<v.game.n;p++){int x=p*30;graphic_frame(x,13,26,12);snprintf(s,sizeof(s),"P%u%s",p+1,adapt_target_ok(p)?"":"X");graphic_text(x+3,22,s);if(p==adapt_target)u8g2_DrawHLine(&u8g2,x,25,26);}
        graphic_text(0,31,"PICK TARGET / TAP:NEXT");
    }else if(adapt_stage==2){
        unsigned min=v.game.kind==PG_CODE?0:2,count=v.game.kind==PG_CODE?12:7;
        for(unsigned i=0;i<count;i++){int x=i*(v.game.kind==PG_CODE?10:18);snprintf(s,sizeof(s),"%u",min+i);graphic_text(x,21,s);if(min+i==adapt_guess)u8g2_DrawHLine(&u8g2,x,24,9);}
        graphic_text(0,31,"PICK NUMBER TAP:NEXT");
    }else{
        if(v.game.kind==PG_CODE)snprintf(s,sizeof(s),"NUM %u",adapt_guess);
        else if(v.game.kind==PG_LOVE)snprintf(s,sizeof(s),"P%u N%u",adapt_target+1,adapt_guess);
        else snprintf(s,sizeof(s),"%s",adapt_card==0?"PLAY":adapt_card==1?"CALL":"ACCEPT");
        graphic_text(0,22,s);graphic_frame(adapt_confirm?88:59,13,adapt_confirm?39:27,12);graphic_text(62,22,"YES");graphic_text(92,22,"BACK");
        graphic_text(0,31,"TAP:CONFIRM BACK:UNDO");
    }
    if(key==BTN_ID_SP){btn.pressed=false;
        if(adapt_stage==1){adapt_stage=v.game.hand[adapt_card]==1?2:3;adapt_confirm=0;}
        else if(adapt_stage==2){adapt_stage=3;adapt_confirm=0;}
        else if(adapt_confirm)adapt_reset();
        else if(v.game.kind==PG_CODE)adapt_commit(PG_GUESS,adapt_card,adapt_target,adapt_guess);
        else if(v.game.kind==PG_LOVE)adapt_commit(PG_CARD,adapt_card,adapt_target,adapt_guess);
        else {unsigned action=adapt_card==0?PG_CARD:adapt_card==1?PG_CHALLENGE:PG_PASS;if(adapt_commit(action,action==PG_CARD?liar_mask:0,0,0))liar_mask=0;}
    }
    return true;
}
static void adapt_hand(void){
    if(adapt_picker())return;
    unsigned nh=v.game.nh>32?32:v.game.nh,total=nh+(v.game.kind==PG_LIAR?3:0);adapt_turn(total);
    if(!nh && v.game.kind!=PG_LIAR){line(18,T("No cards yet","暂时没有手牌"));return;}
    unsigned first=((cell<nh?cell:nh?nh-1:0)/5)*5;
    for(unsigned i=0;i<5 && first+i<nh;i++){
        unsigned pos=first+i,value=v.game.hand[pos];int x=i*20;graphic_frame(x,1,18,22);char number[8];
        if(v.game.kind==PG_CODE){graphic_text(x+3,9,(value&1)?"W":"B");snprintf(number,sizeof(number),"%u",(value&127)/2);}
        else {graphic_text(x+3,9,v.game.kind==PG_LIAR?((liar_mask&(1u<<pos))?"X":"-"):"C");snprintf(number,sizeof(number),"%u",value);}
        graphic_text(x+3,20,number);if(pos==cell)u8g2_DrawHLine(&u8g2,x,24,18);
    }
    char label[48];snprintf(label,sizeof(label),"%u/%u",cell<nh?cell+1:0,nh);graphic_text(103,9,label);
    if(v.game.kind==PG_LIAR){const char *actions[]={"PLAY","CALL","OK"};graphic_text(103,22,cell<nh?"...":actions[cell-nh]);if(cell>=nh)u8g2_DrawHLine(&u8g2,103,24,24);}
    if(v.game.kind==PG_CODE && v.game.extra[0]!=255){unsigned held=v.game.extra[0];snprintf(label,sizeof(label),"H%c%u",held&1?'W':'B',held/2);graphic_text(103,22,label);}
    if(v.game.kind==PG_LOVE){char name[48];card_name(PG_LOVE,v.game.hand[cell%nh],name);ui_zh_draw_fit(&u8g2,46,21,name,81);if(v.game.extra[1]){snprintf(label,sizeof(label),"P%u=%u TAP:PLAY",v.game.extra[0]+1,v.game.extra[1]);graphic_text(0,31,label);}else graphic_text(0,31,"TAP:PLAY BACK:ROOM");}
    else if(v.game.kind==PG_LIAR){snprintf(label,sizeof(label),"R%u PICK/PLAY/CALL/OK",v.game.extra[14]);graphic_text(0,31,label);}
    else graphic_text(0,31,"TURN:VIEW BACK:ROOM");
    adapt_status();
    if(btn.pressed && btn.id==BTN_ID_SP){btn.pressed=false;
        if(v.game.kind==PG_LIAR){if(cell<nh){unsigned bits=0;for(unsigned i=0;i<nh;i++)bits+=(liar_mask>>i)&1;if((liar_mask&(1u<<cell)) || bits<3)liar_mask^=1u<<cell;}else if(adapt_ready() && v.game.me==v.game.turn && (cell!=nh || liar_mask))adapt_begin(cell-nh,0,3);}
        else if(v.game.kind==PG_LOVE && adapt_ready() && v.game.me==v.game.turn){
            unsigned card=v.game.hand[cell];if((card==5 || card==6) && nh==2 && v.game.hand[cell?0:1]==7){notice(T("Play Countess first","请先打出伯爵夫人"));return;}
            adapt_begin(cell,0,3);
            if(card==1 || card==2 || card==3 || card==5 || card==6){bool found=false;for(unsigned p=0;p<v.game.n;p++)if(adapt_target_ok(p)){adapt_target=p;found=true;break;}adapt_stage=found?1:card==1?2:3;}
        }
    }
}
static void adapt_code(void){
    if(adapt_picker())return;
    unsigned total=0,players[64],tiles[64],stride=v.game.n?64/v.game.n:0;
    for(unsigned p=0;p<v.game.n;p++)if(p!=v.game.me)for(unsigned j=0;j<v.game.data[p] && j<stride && total<64;j++){players[total]=p;tiles[total++]=j;}
    unsigned count=total;adapt_turn(total+1);if(!total){line(18,T("No cards yet","暂时没有手牌"));return;}
    if(cell==count){graphic_text(0,8,"END GUESSING");graphic_frame(43,10,40,14);graphic_text(49,21,"PASS");graphic_text(0,31,"TAP:PASS BACK:ROOM");if(btn.pressed && btn.id==BTN_ID_SP){btn.pressed=false;adapt_commit(PG_PASS,0,0,0);}return;}
    unsigned p=players[cell],target=tiles[cell],first=(target/4)*4;char label[40];snprintf(label,sizeof(label),"P%u %.6s %u/%u",p+1,v.names[p],target+1,v.game.data[p]);graphic_text(0,7,label);
    for(unsigned i=0;i<4 && first+i<v.game.data[p] && first+i<stride;i++){
        unsigned value=v.game.board[p*stride+first+i];int x=i*21;graphic_frame(x,9,18,15);
        graphic_text(x+2,16,value==254 || (value<253 && (value&1))?"W":"B");
        if(value>=253)snprintf(label,sizeof(label),"?");else snprintf(label,sizeof(label),"%u",(value&127)/2);
        graphic_text(x+7,22,label);if(first+i==target)u8g2_DrawHLine(&u8g2,x,24,18);
    }
    graphic_text(90,19,"GUESS");graphic_text(0,31,v.game.me==v.game.turn?"TURN:PICK TAP:GUESS":"WAIT / TURN:VIEW");
    adapt_status();
    if(btn.pressed && btn.id==BTN_ID_SP){btn.pressed=false;if(adapt_ready() && v.game.me==v.game.turn && v.game.board[p*stride+target]>=253)adapt_begin(p,target,2);}
}
static const char *adapt_phase(void){
    switch(v.game.phase){case PG_DISCUSS:return "DISCUSS";case PG_VOTING:return "VOTE";case PG_NOMINATE:return "TEAM";case PG_MISSION:return "MISSION";case PG_FINISHED:return "FINISHED";default:return "NIGHT";}
}
static void adapt_players(void){
    unsigned n=v.game.n>8?8:v.game.n;adapt_turn(n);if(!n)return;
    char header[40];if(v.game.kind==PG_AVALON){unsigned picked=0;for(unsigned i=0;i<n;i++)picked+=(v.game.data[0]>>i)&1;snprintf(header,sizeof(header),"%s %u/%u +%u -%u P%u",adapt_phase(),picked,v.game.data[4],v.game.data[1],v.game.data[2],cell+1);}else snprintf(header,sizeof(header),"%s P%u %.6s",adapt_phase(),cell+1,v.names[cell]);graphic_text(0,7,header);
    for(unsigned i=0;i<n;i++){
        int x=i*16;graphic_frame(x,10,13,13);char number[3];snprintf(number,sizeof(number),"%u",i+1);graphic_text(x+4,20,number);
        if(!(v.game.flags[i]&PG_ALIVE))u8g2_DrawHLine(&u8g2,x+1,16,11);
        if(v.game.flags[i]&PG_ACTED)u8g2_DrawBox(&u8g2,x+10,11,2,2);
        if(v.game.kind==PG_AVALON && (v.game.data[0]&(1u<<i)))u8g2_DrawHLine(&u8g2,x,9,13);
        if(i==cell)u8g2_DrawHLine(&u8g2,x,24,13);
    }
    graphic_text(0,31,v.game.phase==PG_DISCUSS?(v.local?"TAP:OPEN VOTE":"DISCUSS / WAIT HOST"):v.game.phase==PG_VOTING?"TURN:TARGET TAP:VOTE":v.game.phase==PG_FINISHED?"TAP:RESULT BACK:ROOM":"TURN:PICK TAP:ACTION");
    if(btn.pressed && btn.id==BTN_ID_SP){btn.pressed=false;if(v.game.phase==PG_FINISHED){view_mode=2;list_enter=true;return;}if(adapt_ready()){
        if(v.game.phase==PG_DISCUSS){if(v.local)request(PARTY_MOVE,PG_ADVANCE,0,0,0);}
        else if(v.game.phase==PG_VOTING && v.game.kind!=PG_AVALON)request(PARTY_MOVE,PG_VOTE,cell,0,0);
        else adapt_open(cell,0);
    }}
}
