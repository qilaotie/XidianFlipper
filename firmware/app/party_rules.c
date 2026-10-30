#include "party_rules.h"
#include <string.h>
#define NONE 255
static uint32_t random32(party_rules_t *g){uint32_t x=g->rng?g->rng:1;x^=x<<13;x^=x>>17;x^=x<<5;return g->rng=x;}
static unsigned bits(unsigned x){unsigned n=0;while(x){n+=x&1;x>>=1;}return n;}
unsigned party_min(unsigned k){static const uint8_t n[]={2,3,2,2,2,2,5,3,5,2,2};return k<TABLETOP_GAMES?n[k]:9;}
unsigned party_max(unsigned k){return k==PG_CODE || k==PG_LIAR || k==PG_LOVE?4:k==PG_TIC || k==PG_REVERSI?2:8;}
static void shuffle(party_rules_t *g,uint8_t *p,unsigned n){while(n>1){unsigned j=random32(g)%n;uint8_t t=p[--n];p[n]=p[j];p[j]=t;}}
static unsigned alive(const party_rules_t *g){unsigned n=0;for(unsigned i=0;i<g->n;i++)n+=(g->flags[i]&PG_ALIVE)!=0;return n;}
static unsigned next(const party_rules_t *g,unsigned seat){for(unsigned i=0;i<g->n;i++){seat=(seat+g->n+g->direction)%g->n;if(g->flags[seat]&PG_ALIVE)return seat;}return seat;}
static void finish(party_rules_t *g,unsigned mask){g->phase=PG_FINISHED;g->winner=(uint8_t)mask;g->event++;}
static bool last_alive(party_rules_t *g){if(alive(g)>1)return false;unsigned mask=0;for(unsigned i=0;i<g->n;i++)if(g->flags[i]&PG_ALIVE)mask|=1u<<i;finish(g,mask);return true;}
static void remove_card(party_rules_t *g,unsigned p,unsigned at){if(at>=g->nh[p])return;memmove(g->hand[p]+at,g->hand[p]+at+1,--g->nh[p]-at);}
static bool take(party_rules_t *g,unsigned p){
    if(g->nh[p]>=PARTY_HAND)return false;
    if(!g->deck_n && g->kind==PG_UNO && g->discard_n>1){uint8_t top=g->discard[--g->discard_n];memcpy(g->deck,g->discard,g->discard_n);g->deck_n=g->discard_n;g->discard[0]=top;g->discard_n=1;shuffle(g,g->deck,g->deck_n);}
    if(!g->deck_n)return false;
    g->hand[p][g->nh[p]++]=g->deck[--g->deck_n];return true;
}
static void votes_clear(party_rules_t *g){memset(g->votes,NONE,sizeof(g->votes));g->acted=0;for(unsigned i=0;i<g->n;i++)g->flags[i]&=(uint8_t)~PG_ACTED;}
static void mark(party_rules_t *g,unsigned p){g->acted|=1u<<p;g->flags[p]|=PG_ACTED;}
static bool all_acted(const party_rules_t *g,bool living){for(unsigned i=0;i<g->n;i++)if((!living || (g->flags[i]&PG_ALIVE)) && !(g->acted&(1u<<i)))return false;return true;}
static void code_insert(party_rules_t *g,unsigned p,uint8_t card,bool reveal){unsigned n=g->nh[p],at=0;if(n>=24)return;while(at<n && g->hand[p][at]<card)at++;memmove(g->hand[p]+at+1,g->hand[p]+at,n-at);g->hand[p][at]=card;g->nh[p]++;uint32_t low=(1u<<at)-1;g->revealed[p]=(g->revealed[p]&low)|((g->revealed[p]&~low)<<1)|(reveal?1u<<at:0);}
static void code_turn(party_rules_t *g){g->held=g->deck_n?g->deck[--g->deck_n]:NONE;g->drawn=0;}
static void liar_deal(party_rules_t *g){g->deck_n=20;for(unsigned i=0;i<18;i++)g->deck[i]=i/6+1;g->deck[18]=g->deck[19]=0;shuffle(g,g->deck,20);for(unsigned p=0;p<g->n;p++){g->nh[p]=0;if(g->flags[p]&PG_ALIVE)for(unsigned i=0;i<5;i++)take(g,p);}g->data[0]=random32(g)%3+1;g->last_count=0;g->last=NONE;g->round++;}
static void love_turn(party_rules_t *g){
    if(last_alive(g))return;
    if(!g->deck_n){unsigned best=0,mask=0;for(unsigned p=0;p<g->n;p++)if(g->flags[p]&PG_ALIVE){unsigned value=g->hand[p][0]*256u+g->score[p];if(value>best){best=value;mask=1u<<p;}else if(value==best)mask|=1u<<p;}finish(g,mask);return;}
    g->turn=next(g,g->turn);g->flags[g->turn]&=(uint8_t)~PG_PROTECTED;take(g,g->turn);
}
static void role_shuffle(party_rules_t *g,uint8_t *roles,unsigned n){shuffle(g,roles,n);for(unsigned i=0;i<g->n;i++)g->role[i]=g->original[i]=roles[i];for(unsigned i=0;i<3 && g->n+i<n;i++)g->center[i]=g->initial_center[i]=roles[g->n+i];}
static void wolf_win(party_rules_t *g){unsigned w=0,c=0,wm=0,cm=0;for(unsigned i=0;i<g->n;i++){if(g->role[i]==1){wm|=1u<<i;if(g->flags[i]&PG_ALIVE)w++;}else{cm|=1u<<i;if(g->flags[i]&PG_ALIVE)c++;}}if(!w)finish(g,cm);else if(w>=c)finish(g,wm);}
static void night_begin(party_rules_t *g){votes_clear(g);g->night_target=g->poison=NONE;g->phase=PG_NIGHT_WOLF;g->round++;for(unsigned i=0;i<g->n;i++)memset(g->extra[i],NONE,16);}
static void night_progress(party_rules_t *g);
static void wolf_dawn(party_rules_t *g){
    unsigned hunter=NONE;
    if(g->night_target<g->n && !(g->data[6]&1)){g->flags[g->night_target]&=(uint8_t)~PG_ALIVE;if(g->role[g->night_target]==4)hunter=g->night_target;}
    if(g->poison<g->n){g->flags[g->poison]&=(uint8_t)~PG_ALIVE;/* poisoned hunter cannot shoot */if(hunter==g->poison)hunter=NONE;}
    g->data[6]=g->data[7]=0;votes_clear(g);g->phase=PG_DISCUSS;
    if(hunter<g->n){g->phase=PG_HUNTER;g->actor=hunter;return;}wolf_win(g);
}
static void night_progress(party_rules_t *g){
    for(unsigned step=0;step<5;step++){
        unsigned role=g->phase==PG_NIGHT_WOLF?1:g->phase==PG_NIGHT_SEER?2:g->phase==PG_NIGHT_ROBBER?3:g->phase==PG_NIGHT_SWAP?4:g->phase==PG_NIGHT_WITCH?3:NONE;
        if(role==NONE)return;
        bool done=true;for(unsigned i=0;i<g->n;i++)if(g->original[i]==role && (g->kind==PG_NIGHT || (g->flags[i]&PG_ALIVE)) && !(g->acted&(1u<<i)))done=false;
        if(!done)return;
        if(g->phase==PG_NIGHT_WOLF && g->kind==PG_WOLF){unsigned counts[8]={0},best=0;g->night_target=NONE;for(unsigned i=0;i<g->n;i++)if(g->original[i]==1 && (g->flags[i]&PG_ALIVE) && g->votes[i]<g->n)counts[g->votes[i]]++;for(unsigned i=0;i<g->n;i++)if(counts[i]>best){best=counts[i];g->night_target=i;}}
        votes_clear(g);
        if(g->phase==PG_NIGHT_WOLF)g->phase=PG_NIGHT_SEER;
        else if(g->phase==PG_NIGHT_SEER)g->phase=g->kind==PG_WOLF?PG_NIGHT_WITCH:PG_NIGHT_ROBBER;
        else if(g->phase==PG_NIGHT_ROBBER)g->phase=PG_NIGHT_SWAP;
        else if(g->phase==PG_NIGHT_WITCH){wolf_dawn(g);return;}
        else {g->phase=PG_DISCUSS;return;}
    }
}
void party_rules_lobby(party_rules_t *g,unsigned kind,uint32_t seed){memset(g,0,sizeof(*g));g->kind=(uint8_t)kind;g->direction=1;g->last=NONE;g->rng=seed?seed:1;g->phase=PG_LOBBY;memset(g->sheets,255,sizeof(g->sheets));}
bool party_rules_start(party_rules_t *g,unsigned n){
    if(!party_game_available(g->kind) || g->phase!=PG_LOBBY || n<party_min(g->kind) || n>party_max(g->kind))return false;
    g->n=n;g->phase=PG_PLAYING;for(unsigned i=0;i<n;i++)g->flags[i]=PG_ALIVE;votes_clear(g);
    switch(g->kind){
    case PG_UNO:
        for(unsigned col=0;col<4;col++){g->deck[g->deck_n++]=col*13;for(unsigned rank=1;rank<13;rank++)for(unsigned j=0;j<2;j++)g->deck[g->deck_n++]=col*13+rank;}
        for(unsigned j=0;j<4;j++){g->deck[g->deck_n++]=52;g->deck[g->deck_n++]=53;}
        shuffle(g,g->deck,g->deck_n);for(unsigned j=0;j<7;j++)for(unsigned p=0;p<n;p++)take(g,p);
        /* Start with a number card; all action cards remain in the deck. */
        for(unsigned i=0;i<g->deck_n;i++)if(g->deck[i]<52 && g->deck[i]%13<10){g->last=g->deck[i];g->deck[i]=g->deck[--g->deck_n];break;}
        g->discard[g->discard_n++]=g->last;g->data[0]=g->last/13;break;
    case PG_UNDERCOVER:{uint8_t roles[8]={1};role_shuffle(g,roles,n);g->data[0]=random32(g)%24;g->phase=PG_DISCUSS;break;}
    case PG_CODE:g->deck_n=24;for(unsigned i=0;i<24;i++)g->deck[i]=i;shuffle(g,g->deck,24);for(unsigned p=0;p<n;p++)for(unsigned j=0;j<4;j++)code_insert(g,p,g->deck[--g->deck_n],false);code_turn(g);break;
    case PG_LIAR:for(unsigned i=0;i<n;i++)g->lives[i]=3;liar_deal(g);break;
    case PG_LOVE:{static const uint8_t deck[]={1,1,1,1,1,2,2,3,3,4,4,5,5,6,7,8};memcpy(g->deck,deck,16);g->deck_n=16;shuffle(g,g->deck,16);g->held=g->deck[--g->deck_n];if(n==2)g->deck_n-=3;for(unsigned p=0;p<n;p++)take(g,p);take(g,0);break;}
    case PG_YACHT:g->round=1;g->rolls=0;break;
    case PG_AVALON:{uint8_t roles[8]={0};unsigned evil=n>=7?3:2;roles[0]=1;roles[1]=2;for(unsigned i=1;i<evil;i++)roles[i+1]=3;role_shuffle(g,roles,n);g->phase=PG_NOMINATE;g->round=1;break;}
    case PG_NIGHT:{uint8_t roles[11]={1,1,2,3,4,0,0,0,0,0,0};role_shuffle(g,roles,n+3);night_begin(g);night_progress(g);break;}
    case PG_WOLF:{uint8_t roles[8]={1,2,3,0,0,0,0,0};if(n>=6)roles[3]=1;if(n>=7)roles[4]=4;role_shuffle(g,roles,n);g->potions=3;night_begin(g);night_progress(g);break;}
    case PG_TIC:memset(g->board,0,64);break;
    case PG_REVERSI:g->board[27]=g->board[36]=2;g->board[28]=g->board[35]=1;break;
    default:return false;
    }return true;
}
static unsigned flips(party_rules_t *g,unsigned pos,unsigned p,bool apply){
    if(pos>=64 || g->board[pos])return 0;
    unsigned total=0;int x=pos%8,y=pos/8;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy){int a=x+dx,b=y+dy;unsigned count=0;while(a>=0&&a<8&&b>=0&&b<8&&g->board[b*8+a]==2-p){count++;a+=dx;b+=dy;}if(count&&a>=0&&a<8&&b>=0&&b<8&&g->board[b*8+a]==p+1){total+=count;if(apply)for(unsigned j=1;j<=count;j++)g->board[(y+dy*(int)j)*8+x+dx*(int)j]=p+1;}}
    return total;
}
static bool reversi_can(party_rules_t *g,unsigned p){for(unsigned i=0;i<64;i++)if(flips(g,i,p,false))return true;return false;}
unsigned party_yacht_score(const uint8_t dice[5],unsigned cat){
    if(cat>=13)return 0;
    unsigned counts[7]={0},sum=0,max=0,run=0,best=0,pair=0,triple=0;
    for(unsigned i=0;i<5;i++){if(dice[i]<1||dice[i]>6)return 0;counts[dice[i]]++;sum+=dice[i];}
    for(unsigned i=1;i<=6;i++){if(counts[i]>max)max=counts[i];if(counts[i]==2)pair++;if(counts[i]==3)triple++;run=counts[i]?run+1:0;if(run>best)best=run;}
    if(cat<6)return counts[cat+1]*(cat+1);
    switch(cat){case 6:return max>=3?sum:0;case 7:return max>=4?sum:0;case 8:return pair&&triple?25:0;case 9:return best>=4?30:0;case 10:return best==5?40:0;case 11:return max==5?50:0;default:return sum;}
}
static bool uno_move(party_rules_t *g,unsigned p,unsigned act,unsigned a,unsigned color){
    if(act==PG_UNO_CALL && g->nh[p]==1){g->flags[p]|=PG_DECLARED;return true;}
    if(act==PG_UNO_CATCH && a<g->n && a!=p && g->nh[a]==1 && !(g->flags[a]&PG_DECLARED)){take(g,a);take(g,a);g->flags[a]|=PG_DECLARED;return true;}
    if(p!=g->turn)return false;
    if(act==PG_DRAW){if(g->drawn)return false;unsigned n=g->penalty?g->penalty:1;for(unsigned i=0;i<n;i++)take(g,p);if(g->penalty){g->penalty=0;g->turn=next(g,p);}else g->drawn=1;return true;}
    if(act==PG_PASS && g->drawn){g->drawn=0;g->turn=next(g,p);return true;}
    if(act!=PG_CARD || a>=g->nh[p] || color>3 || g->penalty || (g->drawn && a+1!=g->nh[p]))return false;
    unsigned card=g->hand[p][a],rank=card%13;
    if(card<52 && card/13!=g->data[0] && (g->last>=52 || rank!=g->last%13))return false;
    if(card==53)for(unsigned i=0;i<g->nh[p];i++)if(g->hand[p][i]<52 && g->hand[p][i]/13==g->data[0])return false;
    remove_card(g,p,a);g->last=card;g->discard[g->discard_n++]=card;g->data[0]=card>=52?color:card/13;g->flags[p]&=(uint8_t)~PG_DECLARED;g->drawn=0;
    if(!g->nh[p]){finish(g,1u<<p);return true;}
    if(card<52 && rank==11)g->direction=-g->direction;
    unsigned to=next(g,p);if(card<52 && (rank==10 || (rank==11 && g->n==2)))to=next(g,to);
    if((card<52 && rank==12)||card==53)g->penalty=card==53?4:2;
    g->turn=to;return true;
}
static bool code_move(party_rules_t *g,unsigned p,unsigned act,unsigned target,unsigned tile,unsigned value){
    if(p!=g->turn)return false;
    if(act==PG_PASS && g->drawn){if(g->held!=NONE)code_insert(g,p,g->held,false);g->turn=next(g,p);code_turn(g);return true;}
    if(act!=PG_GUESS || target>=g->n || target==p || !(g->flags[target]&PG_ALIVE) || tile>=g->nh[target] || value>11 || (g->revealed[target]&(1u<<tile)))return false;
    g->actor=target;g->last=tile;
    if(g->hand[target][tile]/2==value){g->revealed[target]|=1u<<tile;g->event=1;g->drawn=1;if(g->revealed[target]==((1u<<g->nh[target])-1))g->flags[target]&=(uint8_t)~PG_ALIVE;last_alive(g);}
    else {g->event=2;if(g->held!=NONE)code_insert(g,p,g->held,true);else for(unsigned i=0;i<g->nh[p];i++)if(!(g->revealed[p]&(1u<<i))){g->revealed[p]|=1u<<i;break;}if(g->revealed[p]==((1u<<g->nh[p])-1))g->flags[p]&=(uint8_t)~PG_ALIVE;if(!last_alive(g)){g->turn=next(g,p);code_turn(g);}}
    return true;
}
static bool liar_move(party_rules_t *g,unsigned p,unsigned act,unsigned mask){
    if(p!=g->turn)return false;
    if(act==PG_CHALLENGE && g->last<g->n && g->last_count){bool lie=false;for(unsigned i=0;i<g->last_count;i++)if(g->last_cards[i] && g->last_cards[i]!=g->data[0])lie=true;unsigned loser=lie?g->last:p;g->actor=loser;g->event=lie?1:2;g->lives[loser]--;if(!g->lives[loser])g->flags[loser]&=(uint8_t)~PG_ALIVE;if(last_alive(g))return true;g->turn=(g->flags[loser]&PG_ALIVE)?loser:next(g,loser);liar_deal(g);return true;}
    if(act==PG_PASS && g->last<g->n && !g->nh[g->last]){finish(g,1u<<g->last);return true;}
    if(act!=PG_CARD || !mask || (mask>>g->nh[p]) || bits(mask)>3 || (g->last<g->n && !g->nh[g->last]))return false;
    g->last_count=0;for(unsigned i=0;i<g->nh[p];i++)if(mask&(1u<<i))g->last_cards[g->last_count++]=g->hand[p][i];
    for(int i=g->nh[p]-1;i>=0;i--)if(mask&(1u<<i))remove_card(g,p,i);
    g->last=p;g->turn=next(g,p);return true;
}
static bool love_move(party_rules_t *g,unsigned p,unsigned act,unsigned at,unsigned target,unsigned guess){
    if(p!=g->turn || act!=PG_CARD || at>=g->nh[p])return false;
    unsigned card=g->hand[p][at],other=g->hand[p][at?0:1];
    if((card==5||card==6)&&other==7)return false;
    bool needs=card==1||card==2||card==3||card==5||card==6;
    unsigned eligible=0;for(unsigned i=0;i<g->n;i++)if((g->flags[i]&PG_ALIVE)&&!(g->flags[i]&PG_PROTECTED)&&(i!=p||card==5))eligible++;
    if(needs && eligible && (target>=g->n || !(g->flags[target]&PG_ALIVE) || (g->flags[target]&PG_PROTECTED) || (target==p&&card!=5)))return false;
    if(card==1 && (guess<2 || guess>8))return false;
    remove_card(g,p,at);g->score[p]+=card;g->last=card;g->actor=target;g->board[p]=card;
    if(card==8)g->flags[p]&=(uint8_t)~PG_ALIVE;
    else if(card==4)g->flags[p]|=PG_PROTECTED;
    else if(needs && eligible){
        unsigned value=g->hand[target][0];
        if(card==1 && value==guess)g->flags[target]&=(uint8_t)~PG_ALIVE;
        if(card==2){g->extra[p][0]=target;g->extra[p][1]=value;}
        if(card==3){if(g->hand[p][0]<value)g->flags[p]&=(uint8_t)~PG_ALIVE;else if(g->hand[p][0]>value)g->flags[target]&=(uint8_t)~PG_ALIVE;}
        if(card==5){g->score[target]+=value;g->board[target]=value;remove_card(g,target,0);if(value==8)g->flags[target]&=(uint8_t)~PG_ALIVE;else if(!take(g,target))g->hand[target][g->nh[target]++]=g->held;}
        if(card==6){g->hand[target][0]=g->hand[p][0];g->hand[p][0]=value;}
    }
    love_turn(g);return true;
}
static bool yacht_move(party_rules_t *g,unsigned p,unsigned act,unsigned a){
    if(p!=g->turn)return false;
    if(act==PG_ROLL && g->rolls<3){for(unsigned i=0;i<5;i++)if(!(g->keep&(1u<<i)))g->board[i]=random32(g)%6+1;g->rolls++;return true;}
    if(act==PG_KEEP && a<5 && g->rolls){g->keep^=1u<<a;return true;}
    if(act!=PG_SCORE || a>=13 || !g->rolls || g->sheets[p][a]!=65535)return false;
    unsigned score=party_yacht_score(g->board,a);g->sheets[p][a]=score;g->score[p]+=score;
    bool upper=true;unsigned sum=0;for(unsigned i=0;i<6;i++){if(g->sheets[p][i]==65535)upper=false;else sum+=g->sheets[p][i];}if(upper && sum>=63 && !(g->extra[p][0]&1)){g->extra[p][0]|=1;g->score[p]+=35;}
    g->keep=g->rolls=0;memset(g->board,0,5);g->turn=next(g,p);if(!g->turn)g->round++;
    if(g->round>13){unsigned best=0,mask=0;for(unsigned i=0;i<g->n;i++){if(g->score[i]>best){best=g->score[i];mask=1u<<i;}else if(g->score[i]==best)mask|=1u<<i;}finish(g,mask);}return true;
}
static unsigned evil_mask(const party_rules_t *g,bool evil){unsigned mask=0;for(unsigned i=0;i<g->n;i++)if((g->role[i]>=2)==evil)mask|=1u<<i;return mask;}
static unsigned mission_size(const party_rules_t *g){static const uint8_t sizes[4][5]={{2,3,2,3,3},{2,3,4,3,4},{2,3,3,4,4},{3,4,4,5,5}};return sizes[g->n-5][g->round-1];}
static bool avalon_move(party_rules_t *g,unsigned p,unsigned act,unsigned a){
    if(g->phase==PG_NOMINATE){if(p!=g->turn)return false;if(act==PG_SELECT && a<g->n){g->selected^=1u<<a;return true;}if(act!=PG_ADVANCE || bits(g->selected)!=mission_size(g))return false;g->phase=PG_VOTING;votes_clear(g);return true;}
    if(g->phase==PG_VOTING){if(act!=PG_VOTE || a>1 || (g->acted&(1u<<p)))return false;g->votes[p]=a;mark(g,p);if(all_acted(g,false)){unsigned yes=0;for(unsigned i=0;i<g->n;i++)yes+=g->votes[i];if(yes>g->n/2){g->phase=PG_MISSION;g->rejects=0;votes_clear(g);}else{g->rejects++;if(g->rejects>=5)finish(g,evil_mask(g,true));else{g->turn=next(g,g->turn);g->selected=0;g->phase=PG_NOMINATE;votes_clear(g);}}}return true;}
    if(g->phase==PG_MISSION){if(act!=PG_VOTE || a>1 || !(g->selected&(1u<<p)) || (g->acted&(1u<<p)) || (a==0&&g->role[p]<2))return false;g->votes[p]=a;mark(g,p);if((g->acted&g->selected)==g->selected){unsigned fails=0;for(unsigned i=0;i<g->n;i++)if((g->selected&(1u<<i))&&!g->votes[i])fails++;unsigned need=g->n>=7&&g->round==4?2:1;g->board[g->round-1]=fails>=need?2:1;if(fails>=need)g->mission_bad++;else g->mission_ok++;if(g->mission_bad==3)finish(g,evil_mask(g,true));else if(g->mission_ok==3){g->phase=PG_ASSASSIN;for(unsigned i=0;i<g->n;i++)if(g->role[i]==2)g->actor=i;}else{g->round++;g->turn=next(g,g->turn);g->selected=0;g->phase=PG_NOMINATE;votes_clear(g);}}return true;}
    if(g->phase==PG_ASSASSIN && p==g->actor && act==PG_GUESS && a<g->n){finish(g,evil_mask(g,g->role[a]==1));return true;}return false;
}
static unsigned vote_top(const party_rules_t *g,unsigned *maximum){unsigned counts[8]={0},mask=0;*maximum=0;for(unsigned i=0;i<g->n;i++)if(g->votes[i]<g->n)counts[g->votes[i]]++;for(unsigned i=0;i<g->n;i++){if(counts[i]>*maximum){*maximum=counts[i];mask=1u<<i;}else if(counts[i]==*maximum)mask|=1u<<i;}return mask;}
static bool discussion_move(party_rules_t *g,unsigned p,unsigned act,unsigned a){
    if(g->phase==PG_DISCUSS){if(p || act!=PG_ADVANCE)return false;g->phase=PG_VOTING;votes_clear(g);return true;}
    if(g->phase!=PG_VOTING || act!=PG_VOTE || a>=g->n || !(g->flags[p]&PG_ALIVE) || !(g->flags[a]&PG_ALIVE) || (g->acted&(1u<<p)) || (g->kind==PG_UNDERCOVER&&p==a))return false;
    g->votes[p]=a;mark(g,p);if(!all_acted(g,true))return true;
    unsigned maximum,mask=vote_top(g,&maximum);
    if(g->kind==PG_NIGHT){unsigned wolves=0,dead_wolves=0;for(unsigned i=0;i<g->n;i++)if(g->role[i]==1){wolves|=1u<<i;if((mask&(1u<<i))&&maximum>=2)dead_wolves|=1u<<i;}unsigned all=(1u<<g->n)-1;finish(g,wolves?(dead_wolves?all&~wolves:wolves):(maximum<2?all:0));return true;}
    if(bits(mask)!=1){if(g->kind==PG_WOLF){night_begin(g);night_progress(g);}else{g->phase=PG_DISCUSS;votes_clear(g);}return true;}
    unsigned victim=0;while(!(mask&(1u<<victim)))victim++;g->flags[victim]&=(uint8_t)~PG_ALIVE;g->last=victim;
    if(g->kind==PG_UNDERCOVER){unsigned undercover=0,good=0;for(unsigned i=0;i<g->n;i++){if(g->role[i]){if(g->flags[i]&PG_ALIVE)undercover|=1u<<i;}else good|=1u<<i;}if(!undercover)finish(g,good);else if(alive(g)<=2)finish(g,undercover);else{g->round++;g->phase=PG_DISCUSS;votes_clear(g);}}
    else {if(g->role[victim]==4){g->phase=PG_HUNTER;g->actor=victim;g->data[7]=1;return true;}wolf_win(g);if(g->phase!=PG_FINISHED){night_begin(g);night_progress(g);}}
    return true;
}
static bool night_move(party_rules_t *g,unsigned p,unsigned act,unsigned a,unsigned b){
    if(g->phase==PG_HUNTER){if(p!=g->actor || (act!=PG_GUESS&&act!=PG_PASS) || (act==PG_GUESS&&(a>=g->n || !(g->flags[a]&PG_ALIVE))))return false;if(act==PG_GUESS)g->flags[a]&=(uint8_t)~PG_ALIVE;wolf_win(g);if(g->phase!=PG_FINISHED){if(g->data[7]){g->data[7]=0;night_begin(g);night_progress(g);}else g->phase=PG_DISCUSS;}return true;}
    if(g->phase==PG_DISCUSS || g->phase==PG_VOTING)return discussion_move(g,p,act,a);
    unsigned role=g->phase==PG_NIGHT_WOLF?1:g->phase==PG_NIGHT_SEER?2:g->phase==PG_NIGHT_ROBBER?3:g->phase==PG_NIGHT_SWAP?4:g->phase==PG_NIGHT_WITCH?3:NONE;
    if(role==NONE || g->original[p]!=role || (g->acted&(1u<<p)) || (g->kind==PG_WOLF && !(g->flags[p]&PG_ALIVE)))return false;
    if(act==PG_PASS){mark(g,p);night_progress(g);return true;}
    if(g->kind==PG_WOLF){
        if(role==1){if(act!=PG_GUESS || a>=g->n || !(g->flags[a]&PG_ALIVE) || g->role[a]==1)return false;g->votes[p]=a;}
        if(role==2){if(act!=PG_PEEK || a>=g->n || a==p || !(g->flags[a]&PG_ALIVE))return false;g->extra[p][0]=a;g->extra[p][1]=g->role[a]==1;}
        if(role==3){if(act!=PG_CARD || a>2 || (a==1 && (!(g->potions&1)||g->night_target>=g->n)) || (a==2 && (!(g->potions&2)||b>=g->n||!(g->flags[b]&PG_ALIVE))))return false;if(a==1){g->potions&=(uint8_t)~1;g->data[6]|=1;}if(a==2){g->potions&=(uint8_t)~2;g->poison=b;}}
    }else{
        if(role==1){unsigned count=0;for(unsigned i=0;i<g->n;i++)count+=g->original[i]==1;if(act!=PG_PEEK || count!=1 || a>2)return false;g->extra[p][0]=g->center[a];}
        if(role==2){if(act!=PG_PEEK)return false;if(a<g->n){if(a==p)return false;g->extra[p][0]=a;g->extra[p][1]=g->role[a];}else{unsigned x=a-g->n;if(x>2 || b>2 || x==b)return false;g->extra[p][0]=8+x;g->extra[p][1]=g->center[x];g->extra[p][2]=8+b;g->extra[p][3]=g->center[b];}}
        if(role==3){if(act!=PG_SWAP || a>=g->n || a==p)return false;uint8_t t=g->role[p];g->role[p]=g->role[a];g->role[a]=t;g->extra[p][0]=g->role[p];}
        if(role==4){if(act!=PG_SWAP || a>=g->n || b>=g->n || a==b || a==p || b==p)return false;uint8_t t=g->role[a];g->role[a]=g->role[b];g->role[b]=t;}
    }mark(g,p);night_progress(g);return true;
}
bool party_rules_move(party_rules_t *g,unsigned p,unsigned act,unsigned a,unsigned b,unsigned c){
    if(!party_game_available(g->kind) || p>=g->n || g->phase==PG_LOBBY || g->phase==PG_FINISHED)return false;
    switch(g->kind){
    case PG_UNO:return uno_move(g,p,act,a,b);
    case PG_UNDERCOVER:return discussion_move(g,p,act,a);
    case PG_CODE:return code_move(g,p,act,a,b,c);
    case PG_LIAR:return liar_move(g,p,act,a);
    case PG_LOVE:return love_move(g,p,act,a,b,c);
    case PG_YACHT:return yacht_move(g,p,act,a);
    case PG_AVALON:return avalon_move(g,p,act,a);
    case PG_NIGHT:case PG_WOLF:return night_move(g,p,act,a,b);
    case PG_TIC:{if(p!=g->turn || act!=PG_CARD || a>=9 || g->board[a])return false;g->board[a]=p+1;static const uint8_t lines[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};for(unsigned i=0;i<8;i++)if(g->board[lines[i][0]]==p+1 && g->board[lines[i][1]]==p+1 && g->board[lines[i][2]]==p+1){finish(g,1u<<p);return true;}g->round++;if(g->round==9)finish(g,3);else g->turn=1-p;return true;}
    case PG_REVERSI:{if(p!=g->turn || act!=PG_CARD || !flips(g,a,p,false))return false;flips(g,a,p,true);g->board[a]=p+1;g->turn=1-p;if(!reversi_can(g,g->turn)){if(reversi_can(g,p))g->turn=p;else{unsigned counts[2]={0};for(unsigned i=0;i<64;i++)if(g->board[i])counts[g->board[i]-1]++;g->score[0]=counts[0];g->score[1]=counts[1];finish(g,counts[0]==counts[1]?3:counts[0]>counts[1]?1:2);}}return true;}
    default:return false;
    }
}
void party_rules_view(const party_rules_t *g,unsigned p,party_view_t *v){
    memset(v,0,sizeof(*v));v->kind=g->kind;v->phase=g->phase;v->n=g->n;v->turn=g->turn;v->winner=g->winner;v->round=g->round;v->last=g->last;v->actor=g->actor;v->event=g->event;v->direction=g->direction;v->me=p<g->n?p:NONE;memcpy(v->score,g->score,sizeof(v->score));memcpy(v->flags,g->flags,8);memcpy(v->board,g->board,64);memcpy(v->data,g->data,8);
    if(p<g->n){v->nh=g->nh[p];memcpy(v->hand,g->hand[p],32);v->role=g->kind==PG_NIGHT?g->original[p]:g->role[p];memcpy(v->extra,g->extra[p],16);}
    switch(g->kind){
    case PG_UNO:for(unsigned i=0;i<g->n;i++)v->board[i]=g->nh[i];v->data[1]=g->penalty;v->data[2]=g->drawn;break;
    case PG_UNDERCOVER:v->data[0]=0;if(p<g->n){v->extra[0]=g->data[0];v->extra[1]=g->role[p];}break;
    case PG_CODE:memset(v->board,NONE,64);for(unsigned i=0;i<g->n;i++){v->data[i]=g->nh[i];for(unsigned j=0;j<g->nh[i] && j<64/g->n;j++)v->board[i*(64/g->n)+j]=(g->revealed[i]&(1u<<j))?g->hand[i][j]:(g->hand[i][j]&1?254:253);}if(p<g->n){v->extra[0]=g->turn==p?g->held:NONE;for(unsigned i=0;i<g->nh[p];i++)if(g->revealed[p]&(1u<<i))v->hand[i]|=128;}break;
    case PG_LIAR:for(unsigned i=0;i<g->n;i++){v->board[i]=g->nh[i];v->data[i]=g->lives[i];}v->extra[14]=g->data[0];v->extra[15]=g->last_count;v->data[6]=g->data[0];v->data[7]=g->last_count;break;
    case PG_LOVE:if(g->phase==PG_FINISHED)for(unsigned i=0;i<g->n;i++)v->board[8+i]=(g->flags[i]&PG_ALIVE)&&g->nh[i]?g->hand[i][0]:0;break;
    case PG_YACHT:v->data[0]=g->rolls;v->data[1]=g->keep;if(p<g->n)for(unsigned i=0;i<13;i++)v->extra[i]=g->sheets[p][i]==65535?255:(uint8_t)g->sheets[p][i];break;
    case PG_AVALON:v->data[0]=g->selected;v->data[1]=g->mission_ok;v->data[2]=g->mission_bad;v->data[3]=g->rejects;v->data[4]=g->n>=5&&g->round>=1&&g->round<=5?mission_size(g):0;v->data[6]=v->data[7]=0;if(p<g->n && (g->role[p]==1 || g->role[p]>=2))v->extra[0]=evil_mask(g,true);break;
    case PG_NIGHT:if(p<g->n && g->original[p]==1){unsigned mask=0;for(unsigned i=0;i<g->n;i++)if(g->original[i]==1)mask|=1u<<i;v->extra[15]=mask;}break;
    case PG_WOLF:v->data[6]=v->data[7]=0;if(p<g->n){if(g->role[p]==3){v->extra[14]=g->night_target;v->extra[15]=g->potions;}if(g->role[p]==1){unsigned mask=0;for(unsigned i=0;i<g->n;i++)if(g->role[i]==1)mask|=1u<<i;v->extra[15]=mask;}}break;
    default:break;
    }
    if(g->phase>=PG_NIGHT_WOLF && g->phase<=PG_NIGHT_WITCH)for(unsigned i=0;i<g->n;i++)v->flags[i]&=(uint8_t)~PG_ACTED;
    if(g->phase==PG_FINISHED && (g->kind==PG_UNDERCOVER || g->kind==PG_NIGHT || g->kind==PG_WOLF || g->kind==PG_AVALON))for(unsigned i=0;i<g->n;i++)v->board[i]=g->role[i];
}
