"""Production language persistence with fake NVS; no hardware fault-injection claim."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/ui/ui_language.c').read_text(encoding='utf8'),flags=re.M)
shim=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef int nvs_handle_t;typedef int esp_err_t;
enum {ESP_OK,ESP_ERR_NVS_NOT_FOUND,NVS_READONLY,NVS_READWRITE};
static uint8_t disk[4],stage[4];static bool exists,fail_open,fail_commit;static size_t disk_size=4;static unsigned writes;
static int nvs_open(const char *ns,int mode,int *h){assert(!strcmp(ns,"ui_locale"));*h=1;if(fail_open)return 9;return !exists&&mode==NVS_READONLY?ESP_ERR_NVS_NOT_FOUND:ESP_OK;}
static int nvs_get_blob(int h,const char *key,void *p,size_t *n){(void)h;assert(!strcmp(key,"language_v1"));assert(*n>=disk_size);memcpy(p,disk,disk_size);*n=disk_size;return ESP_OK;}
static int nvs_set_blob(int h,const char *key,const void *p,size_t n){(void)h;assert(!strcmp(key,"language_v1")&&n==4);memcpy(stage,p,n);return ESP_OK;}
static int nvs_commit(int h){(void)h;if(fail_commit)return 9;memcpy(disk,stage,4);exists=true;writes++;return ESP_OK;}
static void nvs_close(int h){(void)h;}
'''
tests=r'''
int main(void){
 assert(ui_language_chinese()&&!exists&&writes==0);
 assert(ui_language_set_chinese(false)&&!ui_language_chinese()&&writes==1);initialized=false;assert(!ui_language_chinese());
 fail_commit=true;assert(!ui_language_set_chinese(true)&&!ui_language_chinese()&&writes==1);initialized=false;assert(!ui_language_chinese());
 fail_commit=false;assert(ui_language_set_chinese(true)&&writes==2);initialized=false;assert(ui_language_chinese());
 assert(ui_language_set_chinese(true)&&writes==2);
 disk[0]=99;initialized=false;assert(ui_language_chinese()&&!ui_language_set_chinese(false)&&disk[0]==99&&writes==2);
 disk[0]=1;disk[1]=3;initialized=false;assert(!ui_language_set_chinese(false)&&writes==2);
 disk[1]=0;disk_size=3;initialized=false;assert(!ui_language_set_chinese(false)&&writes==2);
 disk_size=4;fail_open=true;initialized=false;assert(!ui_language_set_chinese(false)&&writes==2);
 puts("PASS language: Chinese default/no write, EN/ZH reboot persistence, failed writes retain choice, unknown/corrupt records preserved, isolated namespace");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-language-') as t:
    p=Path(t);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector',str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
    subprocess.run([str(p/'test.exe')],check=True)
