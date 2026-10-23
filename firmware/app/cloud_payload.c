#include "cloud_payload.h"
#include <stdio.h>
#include <string.h>
void cloud_hex(char *out,const uint8_t *bytes,size_t count){
    static const char digits[]="0123456789abcdef";
    for(size_t i=0;i<count;i++){out[2*i]=digits[bytes[i]>>4];out[2*i+1]=digits[bytes[i]&15];}out[2*count]=0;
}
bool cloud_payload(char *out,size_t capacity,const char *id,const char *nickname,
                   const pet_t *p,const cloud_score_t scores[2]){
    if(!out || !capacity || !id || strlen(id)!=32 || !nickname || !p || !pet_valid(p))return false;
    for(unsigned i=0;i<32;i++)if(!((id[i]>='0'&&id[i]<='9')||(id[i]>='a'&&id[i]<='f')))return false;
    char name[17];unsigned n=0;
    for(;n<16 && nickname[n];n++){unsigned char c=(unsigned char)nickname[n];name[n]=
        ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='_'||c=='.'||c=='-')?(char)c:'_';}
    name[n]=0;if(!n)snprintf(name,sizeof(name),"Player");
    char games[160]={0};size_t used=0;
    for(unsigned k=0;k<2;k++)if(scores[k].present){
        if((k==1 && (scores[k].score>189 || scores[k].progress!=scores[k].score+3)) ||
           (k==0 && (scores[k].score>100000000 || scores[k].progress>1000000)))return false;
        int size=snprintf(games+used,sizeof(games)-used,"%s{\"game\":\"%s\",\"score\":%lu,\"progress\":%lu}",
                          used?",":"",k?"snake":"tetris",(unsigned long)scores[k].score,(unsigned long)scores[k].progress);
        if(size<0 || (size_t)size>=sizeof(games)-used)return false;
        used+=(size_t)size;
    }
    int size=snprintf(out,capacity,"{\"id\":\"%s\",\"nickname\":\"%s\",\"pet\":{\"age\":%lu,\"form\":%u,\"coins\":%u,\"dex\":%u,\"decor\":%u,\"fullness\":%u,\"mood\":%u,\"energy\":%u,\"clean\":%u,\"sleeping\":%s},\"scores\":[%s]}",
                      id,name,(unsigned long)p->age,p->form,p->coins,p->dex,p->decor,p->fullness,p->mood,p->energy,p->clean,p->sleeping?"true":"false",games);
    return size>=0 && (size_t)size<capacity;
}
