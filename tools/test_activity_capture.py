"""Real result capture and ring: eviction must not replay an old live result."""
from pathlib import Path
import re,subprocess,tempfile
from test_draft_store import shim
root=Path(__file__).resolve().parents[1]
shim=shim.replace('draft_store.h','message_service.h').replace('msg_drafts','activity_log').replace('drafts_v1','history_v1')
store=re.sub(r'^#include[^\n]*\n','',(root/'firmware/app/activity_log.c').read_text(encoding='utf8'),flags=re.M)
service=(root/'firmware/app/message_service.c').read_text(encoding='utf8')
def extract(signature):
 start=service.index(signature);brace=service.index('{',start);end=brace+1;depth=1
 while depth:
  depth+=(service[end]=='{')-(service[end]=='}');end+=1
 return service[start:end]+'\n'
setup=r'''
static team_activity_t s_team;static crowd_activity_t s_crowd;static uint8_t s_channel=1;
static uint32_t s_log_retry_at,clock_ms=1000;
static struct{uint32_t boot,id;uint8_t host[6];}s_recorded[7];
static uint32_t now_ms(void){return clock_ms;}
'''
tests=r'''
int main(void){
 s_team.poll=(team_poll_t){.active=true,.open=false,.boot=100,.id=1,.host={2,0,0,0,0,1},.question="Poll"};record_activities();assert(writes==1);
 for(unsigned i=1;i<=20;i++){s_crowd.rooms[1]=(crowd_room_t){.active=true,.open=false,.boot=100,.id=i,.host={2,0,0,0,0,1},.title="Quiz"};record_activities();}
 assert(writes==21&&activity_log_count()==12);activity_record_t r;for(unsigned i=0;i<12;i++){assert(activity_log_get(i,&r));assert(r.kind==ACTIVITY_QUIZ&&r.id==20-i);}
 record_activities();assert(writes==21);
 fail_commit=true;s_crowd.rooms[1].id=21;record_activities();assert(writes==21&&s_log_retry_at==11000&&s_recorded[6].id==20);
 fail_commit=false;record_activities();assert(writes==21);clock_ms=11001;record_activities();assert(writes==22&&s_recorded[6].id==21&&!activity_log_error());
 puts("PASS capture: eviction watermark, no duplicate writes, failed-save preservation and bounded retry");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(shim+store+setup+extract('static bool save_activity')+extract('static void record_activities')+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'firmware/app'),'-I'+str(root/'firmware/protocol'),str(p/'test.c'),str(root/'build/mesh-packet-host.o'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
