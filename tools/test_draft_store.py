"""Production NVS drafts under simulated failures; not real power-loss evidence."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/ui/draft_store.c').read_text(encoding='utf8'),flags=re.M)
shim=r'''
#include "draft_store.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef int nvs_handle_t;typedef int esp_err_t;
enum{ESP_OK,ESP_ERR_NVS_NOT_FOUND,NVS_READONLY,NVS_READWRITE};
static unsigned char disk[2048],stage[2048];
static size_t disk_size,stage_size;static unsigned writes;
static bool exists,fail_open,fail_set,fail_commit,fail_get;
static int nvs_open(const char *ns,int mode,int *h){assert(!strcmp(ns,"msg_drafts"));*h=1;if(fail_open)return 9;return !exists&&mode==NVS_READONLY?ESP_ERR_NVS_NOT_FOUND:ESP_OK;}
static int nvs_get_blob(int h,const char *key,void *p,size_t *n){(void)h;assert(!strcmp(key,"drafts_v1"));if(fail_get||*n<disk_size)return 9;memcpy(p,disk,disk_size);*n=disk_size;return ESP_OK;}
static int nvs_set_blob(int h,const char *key,const void *p,size_t n){(void)h;assert(!strcmp(key,"drafts_v1")&&n<=sizeof(stage));if(fail_set)return 9;memcpy(stage,p,n);stage_size=n;return ESP_OK;}
static int nvs_commit(int h){(void)h;if(fail_commit)return 9;memcpy(disk,stage,stage_size);disk_size=stage_size;exists=true;writes++;return ESP_OK;}
static void nvs_close(int h){(void)h;}
'''
tests=r'''
int main(void){
 assert(!draft_store_work()[0]&&!writes&&!exists);
 assert(draft_store_save_work("Hello (^_^)"));
 for(unsigned i=0;i<DRAFT_SLOTS;i++){char s[32];snprintf(s,sizeof(s),"Phrase %u",i);assert(draft_store_save_slot(i,s));}
 initialized=false;assert(!strcmp(draft_store_work(),"Hello (^_^)"));
 assert(!strcmp(draft_store_slot(5),"Phrase 5"));
 unsigned before=writes;assert(draft_store_save_work("Hello (^_^)")&&writes==before);
 fail_set=true;assert(!draft_store_save_slot(0,"bad")&&!strcmp(draft_store_slot(0),"Phrase 0"));fail_set=false;
 fail_commit=true;assert(!draft_store_save_work("bad")&&!strcmp(draft_store_work(),"Hello (^_^)"));
 initialized=false;assert(!strcmp(draft_store_work(),"Hello (^_^)"));fail_commit=false;
 assert(!draft_store_save_slot(6,"bad")&&!draft_store_save_work("\n"));
 char max[130];memset(max,'A',128);max[128]=0;assert(draft_store_save_slot(0,max));
 max[128]='A';max[129]=0;assert(!draft_store_save_work(max));
 assert(draft_store_save_slot(1,""));initialized=false;assert(!draft_store_slot(1)[0]&&strlen(draft_store_slot(0))==128);
 before=writes;unsigned char old=disk[0];disk[0]=99;initialized=false;
 assert(!draft_store_work()[0]&&!draft_store_save_work("x")&&writes==before&&disk[0]==99);disk[0]=old;
 disk_size--;initialized=false;assert(!draft_store_save_work("x"));disk_size++;
 memset(disk+1,'A',129);initialized=false;assert(!draft_store_save_work("x"));
 fail_open=true;initialized=false;assert(!draft_store_save_slot(0,"x"));fail_open=false;
 puts("PASS drafts: six slots/work reboot, max length, ASCII validation, no-op writes, failed set/commit, corrupt/future records retained");
}
'''
if __name__=='__main__':
 with tempfile.TemporaryDirectory(prefix='halfstep-drafts-') as tmp:
  p=Path(tmp);(p/'test.c').write_text(shim+source+tests,encoding='utf8')
  subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/ui'),str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
  subprocess.run([str(p/'test.exe')],check=True)
