#include "wifi_credentials.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    wifi_credentials_t profile={.version=1,.security=WIFI_CRED_PSK};
    strcpy(profile.ssid,"TEST_NETWORK");strcpy(profile.password,"12345678");
    assert(wifi_credentials_valid(&profile));
    profile.automatic=1;assert(wifi_credentials_valid(&profile));
    profile.automatic=2;assert(!wifi_credentials_valid(&profile));profile.automatic=0;
    profile.version=7;assert(!wifi_credentials_valid(&profile));profile.version=1;
    profile.password[7]=0;assert(!wifi_credentials_valid(&profile));
    memset(profile.password,'x',63);profile.password[63]=0;assert(wifi_credentials_valid(&profile));
    memset(profile.password,'a',64);profile.password[64]=0;assert(wifi_credentials_valid(&profile));
    profile.password[32]='z';assert(!wifi_credentials_valid(&profile));
    memset(profile.password,'x',65);assert(!wifi_credentials_valid(&profile));
    memset(profile.password,0,65);profile.security=WIFI_CRED_OPEN;assert(wifi_credentials_valid(&profile));
    profile.password[0]='a';assert(!wifi_credentials_valid(&profile));profile.password[0]=0;
    memset(profile.ssid,'x',32);profile.ssid[32]=0;assert(wifi_credentials_valid(&profile));
    profile.ssid[32]='x';assert(!wifi_credentials_valid(&profile));
    memset(profile.ssid,0,33);assert(!wifi_credentials_valid(&profile));
    puts("PASS: Wi-Fi profile version, SSID bound, WPA length/hex, open AP, auto flag; no secrets printed");
}
