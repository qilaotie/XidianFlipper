"""Host test of the production JSON serializer; no device/cloud writes."""
from pathlib import Path
import json
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
test=r'''
#include "cloud_payload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
 uint8_t bytes[16]={0,1,2,255};char id[33],out[640];cloud_hex(id,bytes,16);
 assert(!strcmp(id,"000102ff000000000000000000000000"));
 pet_t p;pet_new(&p);cloud_score_t scores[2]={{true,120,1},{true,4,7}};
 assert(cloud_payload(out,sizeof(out),id,"A\"\\<>",&p,scores));puts(out);
 assert(!strstr(out,"password") && !strstr(out,"token") && !strstr(out,"ssid"));
 assert(!cloud_payload(out,10,id,"Player",&p,scores));
 scores[1].progress=8;assert(!cloud_payload(out,sizeof(out),id,"Player",&p,scores));
 scores[1].progress=7;scores[0].score=100000001;assert(!cloud_payload(out,sizeof(out),id,"Player",&p,scores));
 scores[0].score=120;id[0]='z';assert(!cloud_payload(out,sizeof(out),id,"Player",&p,scores));
 id[0]='0';p.fullness=101;assert(!cloud_payload(out,sizeof(out),id,"Player",&p,scores));
 pet_new(&p);scores[0].present=scores[1].present=false;
 assert(cloud_payload(out,sizeof(out),id,"",&p,scores));puts(out);
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-cloud-') as tmp:
    tmp=Path(tmp);(tmp/'test.c').write_text(test,encoding='utf8')
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'firmware/app'),
                    str(tmp/'test.c'),str(root/'firmware/app/cloud_payload.c'),str(root/'firmware/app/pet.c'),'-o',str(tmp/'test.exe')],check=True)
    rows=subprocess.check_output([str(tmp/'test.exe')],text=True).splitlines()
    first,second=map(json.loads,rows)
    assert first['nickname']=='A____' and first['scores'][1]['game']=='snake'
    assert first['scores'][1]['progress']==7 and second['nickname']=='Player' and second['scores']==[]
    print('PASS production cloud payload: JSON, identity, nickname filtering, score bounds, capacity and privacy whitelist')
