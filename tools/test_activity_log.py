"""Real bounded history store under fake NVS faults; not hardware power-loss proof."""
from pathlib import Path
import re,subprocess,tempfile
from test_draft_store import shim
root=Path(__file__).resolve().parents[1]
shim=shim.replace('draft_store.h','activity_log.h').replace('msg_drafts','activity_log').replace('drafts_v1','history_v1')
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/app/activity_log.c').read_text(encoding='utf8'),flags=re.M)
tests=r'''
static void reboot(void){s_ready=s_error=s_blocked=false;memset(&s_log,0,sizeof(s_log));}
int main(void){
 activity_record_t r={.kind=ACTIVITY_POLL,.channel=1,.boot=100,.id=1,.host={2,0,0,0,0,1},.title="Test"},out;
 assert(activity_log_init()&&!activity_log_count()&&!writes);assert(activity_log_append(&r));unsigned old=writes;
 assert(activity_log_append(&r)&&writes==old);reboot();assert(activity_log_count()==1&&activity_log_get(0,&out)&&out.id==1);
 fail_commit=true;r.id=2;assert(!activity_log_append(&r)&&activity_log_count()==1&&activity_log_error());fail_commit=false;
 reboot();assert(activity_log_count()==1);assert(activity_log_append(&r)&&!activity_log_error());
 for(unsigned i=3;i<=20;i++){r.id=i;assert(activity_log_append(&r));}
 assert(activity_log_count()==12&&activity_log_get(0,&out)&&out.id==20&&activity_log_get(11,&out)&&out.id==9&&!activity_log_get(12,&out));
 reboot();assert(activity_log_count()==12);r.id=21;r.title[0]='\n';assert(!activity_log_append(&r));r.title[0]='T';
 old=writes;disk[0]=99;reboot();assert(!activity_log_init()&&activity_log_error()&&!activity_log_append(&r)&&writes==old&&disk[0]==99);
 puts("PASS history: 12-entry ring, dedup, reboot, failed commit preservation, invalid/future data blocked without erase");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(shim+'\n#include "mesh_packet.h"\n'+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),str(root/'build/mesh-packet-host.o'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
