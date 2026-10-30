#include "party_rules.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static party_rules_t g,before;
static unsigned rng=239831;
static unsigned rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static bool move(unsigned p,unsigned act,unsigned a,unsigned b,unsigned c){
    before=g;bool ok=party_rules_move(&g,p,act,a,b,c);
    if(!ok)assert(!memcmp(&g,&before,sizeof(g)));
    assert(g.n<=8 && g.deck_n<=108 && g.discard_n<=108);
    for(unsigned i=0;i<g.n;i++)assert(g.nh[i]<=32);
    return ok;
}
static void start(unsigned k,unsigned n,unsigned seed){party_rules_lobby(&g,k,seed);assert(party_rules_start(&g,n));}
static unsigned wolf_seat(void){for(unsigned i=0;i<g.n;i++)if(g.role[i]==1 && (g.flags[i]&PG_ALIVE))return i;return 255;}
static void play_to_end(unsigned kind,unsigned n,unsigned seed){
    start(kind,n,seed);
    for(unsigned step=0;step<4000 && g.phase!=PG_FINISHED;step++){
        bool ok=false;unsigned p=g.turn;
        switch(kind){
        case PG_UNO:
            if(g.penalty)ok=move(p,PG_DRAW,0,0,0);
            else {for(unsigned i=0;i<g.nh[p]&&!ok;i++)ok=move(p,PG_CARD,i,rnd()%4,0);if(!ok)ok=move(p,g.drawn?PG_PASS:PG_DRAW,0,0,0);}break;
        case PG_CODE:
            for(unsigned target=0;target<n&&!ok;target++)if(target!=p && (g.flags[target]&PG_ALIVE))for(unsigned tile=0;tile<g.nh[target]&&!ok;tile++)if(!(g.revealed[target]&(1u<<tile)))ok=move(p,PG_GUESS,target,tile,g.hand[target][tile]/2);break;
        case PG_LIAR:if(g.last_count && (rnd()%2 || !g.nh[g.last]))ok=move(p,PG_CHALLENGE,0,0,0);else ok=move(p,PG_CARD,1,0,0);break;
        case PG_LOVE:for(unsigned card=0;card<g.nh[p]&&!ok;card++)for(unsigned target=0;target<n&&!ok;target++)for(unsigned guess=2;guess<=8&&!ok;guess++)ok=move(p,PG_CARD,card,target,guess);break;
        case PG_YACHT:if(!g.rolls)ok=move(p,PG_ROLL,0,0,0);else for(unsigned cat=0;cat<13&&!ok;cat++)ok=move(p,PG_SCORE,cat,0,0);break;
        case PG_UNDERCOVER:if(g.phase==PG_DISCUSS)ok=move(0,PG_ADVANCE,0,0,0);else{unsigned w=wolf_seat();for(unsigned i=0;i<n;i++)if((g.flags[i]&PG_ALIVE)&&!(g.acted&(1u<<i))){unsigned target=i==w?(i+1)%n:w;if(target==i || !(g.flags[target]&PG_ALIVE))for(unsigned j=0;j<n;j++)if(j!=i&&(g.flags[j]&PG_ALIVE)){target=j;break;}ok=move(i,PG_VOTE,target,0,0);break;}}break;
        case PG_AVALON:
            if(g.phase==PG_NOMINATE){party_view_t v;party_rules_view(&g,0,&v);unsigned selected=0;for(unsigned i=0;i<n;i++)selected+=(g.selected>>i)&1;if(selected<v.data[4]){for(unsigned i=0;i<n;i++)if(!(g.selected&(1u<<i))){ok=move(p,PG_SELECT,i,0,0);break;}}else ok=move(p,PG_ADVANCE,0,0,0);}
            else if(g.phase==PG_VOTING || g.phase==PG_MISSION){for(unsigned i=0;i<n;i++)if(!(g.acted&(1u<<i))&&(g.phase==PG_VOTING || g.selected&(1u<<i))){ok=move(i,PG_VOTE,1,0,0);break;}}
            else if(g.phase==PG_ASSASSIN){unsigned target=0;while(g.role[target]==1)target++;ok=move(g.actor,PG_GUESS,target,0,0);}break;
        case PG_NIGHT:case PG_WOLF:
            if(g.phase==PG_DISCUSS)ok=move(0,PG_ADVANCE,0,0,0);
            else if(g.phase==PG_VOTING){unsigned w=wolf_seat();for(unsigned i=0;i<n;i++)if((g.flags[i]&PG_ALIVE)&&!(g.acted&(1u<<i))){ok=move(i,PG_VOTE,w<n?w:i,0,0);break;}}
            else if(g.phase==PG_HUNTER)ok=move(g.actor,PG_PASS,0,0,0);
            else{for(unsigned i=0;i<n&&!ok;i++){if(kind==PG_WOLF && g.phase==PG_NIGHT_WOLF && g.role[i]==1 && (g.flags[i]&PG_ALIVE))for(unsigned target=0;target<n&&!ok;target++)if(g.role[target]!=1&&(g.flags[target]&PG_ALIVE))ok=move(i,PG_GUESS,target,0,0);if(!ok)ok=move(i,PG_PASS,0,0,0);}}break;
        case PG_TIC:case PG_REVERSI:for(unsigned pos=0;pos<(kind==PG_TIC?9u:64u)&&!ok;pos++)ok=move(p,PG_CARD,pos,0,0);break;
        }
        assert(ok);
    }
    if(g.phase!=PG_FINISHED)printf("NOT FINISHED kind%u seed%u\n",kind,seed);
    assert(g.phase==PG_FINISHED);
}
static void targeted(void){
    start(PG_LIAR,3,22);assert(move(0,PG_CARD,1,0,0));party_view_t spectator;party_rules_view(&g,255,&spectator);assert(!spectator.nh && spectator.me==255 && spectator.data[6]==g.data[0] && spectator.data[7]==1);
    uint8_t dice[]={2,2,2,5,5};assert(party_yacht_score(dice,8)==25&&party_yacht_score(dice,6)==16&&party_yacht_score(dice,11)==0);
    uint8_t run[]={1,2,3,4,6};assert(party_yacht_score(run,9)==30&&party_yacht_score(run,10)==0);
    uint8_t yacht[]={6,6,6,6,6};assert(party_yacht_score(yacht,11)==50&&party_yacht_score(yacht,5)==30);
    start(PG_UNO,2,2);g.hand[0][0]=53;g.hand[0][1]=g.data[0]*13;g.nh[0]=2;assert(!move(0,PG_CARD,0,1,0));assert(!move(1,PG_CARD,0,0,0));assert(move(0,PG_DRAW,0,0,0));assert(!move(0,PG_DRAW,0,0,0));assert(move(0,PG_PASS,0,0,0));
    start(PG_YACHT,2,7);assert(move(0,PG_ROLL,0,0,0));uint8_t old[5];memcpy(old,g.board,5);for(unsigned i=0;i<5;i++)assert(move(0,PG_KEEP,i,0,0));assert(move(0,PG_ROLL,0,0,0));assert(!memcmp(old,g.board,5));assert(move(0,PG_ROLL,0,0,0));assert(!move(0,PG_ROLL,0,0,0));assert(move(0,PG_SCORE,0,0,0));
    party_rules_lobby(&g,PG_REVERSI,1);assert(!party_rules_start(&g,2));assert(!party_rules_move(&g,0,PG_CARD,19,0,0));
}
int main(void){
    targeted();
    for(unsigned kind=0;kind<TABLETOP_GAMES;kind++){
        if(!party_game_available(kind))continue;
        for(unsigned seed=1;seed<=40;seed++)play_to_end(kind,party_min(kind)+(seed%(party_max(kind)-party_min(kind)+1)),seed);
        for(unsigned seed=1;seed<=10;seed++){
            start(kind,party_min(kind),seed);for(unsigned i=0;i<800;i++)move(rnd()%9,rnd()%18,rnd()%40,rnd()%12,rnd()%14);
        }
        printf("PASS rules game %u: full matches, bounds, invalid-action atomicity\n",kind);
    }
    puts("PASS 10 active rules: 400 completed games and 80000 randomized attempts; withdrawn board game rejected");
}
