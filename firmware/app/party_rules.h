#ifndef PARTY_RULES_H
#define PARTY_RULES_H
#include <stdbool.h>
#include <stdint.h>
#define PARTY_PLAYERS 8
#define PARTY_HAND 32
enum {PG_UNO,PG_UNDERCOVER,PG_CODE,PG_LIAR,PG_LOVE,PG_YACHT,PG_AVALON,PG_NIGHT,PG_WOLF,PG_TIC,PG_REVERSI,TABLETOP_GAMES};
/* Keep wire IDs stable. Reversi alone is withdrawn for small-screen legibility. */
static inline bool party_game_available(unsigned kind){return kind<TABLETOP_GAMES && kind!=PG_REVERSI;}
static inline bool party_game_graphical(unsigned kind){return kind==PG_UNO || kind==PG_YACHT || kind==PG_TIC;}
enum {PG_LOBBY,PG_PLAYING,PG_DISCUSS,PG_VOTING,PG_NOMINATE,PG_MISSION,PG_ASSASSIN,PG_NIGHT_WOLF,PG_NIGHT_SEER,PG_NIGHT_ROBBER,PG_NIGHT_SWAP,PG_NIGHT_WITCH,PG_HUNTER,PG_FINISHED=255};
enum {PG_CARD=1,PG_DRAW,PG_PASS,PG_GUESS,PG_CHALLENGE,PG_VOTE,PG_ROLL,PG_KEEP,PG_SCORE,PG_SELECT,PG_PEEK,PG_SWAP,PG_ADVANCE,PG_UNO_CALL,PG_UNO_CATCH};
enum {PG_ALIVE=1,PG_PROTECTED=2,PG_DECLARED=4,PG_ACTED=8};
typedef struct {
    uint8_t kind,phase,n,turn,winner,round,last,actor,event;
    int8_t direction;
    uint16_t score[8];uint8_t flags[8],data[8],board[64];
    uint8_t me,nh,hand[32],role,extra[16];
} party_view_t;
typedef struct {
    uint8_t kind,phase,n,turn,winner,round,last,actor,event;
    int8_t direction;
    uint16_t score[8],sheets[8][13];
    uint8_t flags[8],data[8],board[64],nh[8],hand[8][32],role[8],original[8],extra[8][16];
    uint8_t deck[108],deck_n,discard[108],discard_n,votes[8],center[3],initial_center[3];
    uint32_t revealed[8],rng;uint8_t selected,acted,held,keep,rolls,drawn,penalty;
    uint8_t last_cards[3],last_count,lives[8],mission_ok,mission_bad,rejects,night_target,poison,potions;
} party_rules_t;
unsigned party_min(unsigned kind);
unsigned party_max(unsigned kind);
void party_rules_lobby(party_rules_t *,unsigned kind,uint32_t seed);
bool party_rules_start(party_rules_t *,unsigned count);
bool party_rules_move(party_rules_t *,unsigned seat,unsigned action,unsigned a,unsigned b,unsigned c);
void party_rules_view(const party_rules_t *,unsigned seat,party_view_t *);
unsigned party_yacht_score(const uint8_t dice[5],unsigned category);
#endif
