"""Production style NVS isolation, strict validation and failed commit coverage."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
language=(root/'tools/test_ui_language.py').read_text(encoding='utf8')
shim=language.split("shim=r'''",1)[1].split("'''",1)[0]
shim=shim.replace('ui_locale','ui_personal').replace('language_v1','style_v1')
source=re.sub(r'^#include[^\n]*\n','',(root/'firmware/ui/ui_style.c').read_text(encoding='utf8'),flags=re.M)
tests=r'''
int main(void){
 assert(ui_style_skin()==0&&ui_style_widget()==0&&!exists&&writes==0);
 for(unsigned s=0;s<UI_SKINS;s++)for(unsigned w=0;w<UI_WIDGETS;w++){
  assert(ui_style_set(s,w));initialized=false;assert(ui_style_skin()==s&&ui_style_widget()==w);
 }
 unsigned before=writes;assert(ui_style_set(2,3)&&writes==before);
 fail_commit=true;assert(!ui_style_set(0,0)&&ui_style_skin()==2&&ui_style_widget()==3);initialized=false;
 assert(ui_style_skin()==2&&ui_style_widget()==3&&writes==before);fail_commit=false;
 assert(!ui_style_set(UI_SKINS,0)&&!ui_style_set(0,UI_WIDGETS));
 for(unsigned i=0;i<4;i++){
  uint8_t old=disk[i];disk[i]=255;initialized=false;
  assert(ui_style_skin()==0&&!ui_style_set(1,1)&&writes==before);disk[i]=old;
 }
 disk_size=3;initialized=false;assert(!ui_style_set(1,1));disk_size=4;
 fail_open=true;initialized=false;assert(!ui_style_set(1,1));
 puts("PASS styles: 12 choices/reboot, default no writes, no-op, failed commit, corrupt records retained, isolated namespace");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-style-') as t:
 p=Path(t);(p/'test.c').write_text('#include "ui_style.h"\n'+shim+source+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/ui'),str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
