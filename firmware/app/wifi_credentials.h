#ifndef WIFI_CREDENTIALS_H
#define WIFI_CREDENTIALS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
enum { WIFI_CRED_OPEN,WIFI_CRED_PSK };
typedef struct {
    uint8_t version,security,automatic,reserved;
    char ssid[33],password[65];
} wifi_credentials_t;
static inline bool wifi_credentials_valid(const wifi_credentials_t *p){
    if(!p || p->version!=1 || p->security>WIFI_CRED_PSK || p->automatic>1 || p->reserved ||
       !memchr(p->ssid,0,sizeof(p->ssid)) || !p->ssid[0] || !memchr(p->password,0,sizeof(p->password)))return false;
    size_t length=strlen(p->password);
    if(p->security==WIFI_CRED_OPEN)return length==0;
    if(length<8)return false;
    for(size_t i=0;i<length;i++){
        unsigned char c=(unsigned char)p->password[i];
        if(c<32 || c>126)return false;
        if(length==64 && !((c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F')))return false;
    }
    return true;
}
#endif
