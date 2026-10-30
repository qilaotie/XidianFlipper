"""Exercise production editor/emote/draft actions with fake input, storage and display."""
from pathlib import Path
import subprocess,tempfile
from test_draft_store import shim,source
root=Path(__file__).resolve().parents[1]
local=(root/'firmware/ui/local_pages.c').read_text(encoding='utf8')
def function(name):
 start=local.index('static void '+name+'(');brace=local.index('{',start);depth=1;end=brace+1
 while depth:
  depth+=(local[end]=='{')-(local[end]=='}');end+=1
 return local[start:end]+'\n'
ui=r'''
#include "message_draft.h"
#include "message_emotes.h"
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define MESSAGE_TEXT_MAX 128
enum{M_MESSAGE_COMPOSE=47,M_MESSAGE_EMOTES=50,M_DRAFT_BOX=64,M_DRAFT_SLOT,M_DRAFT_CONFIRM,M_MESSAGE_EMOTE_LIST,M_RECIPIENT_PICK=29,M_REPLY_CONFIRM=86,M_CONTENT_EDIT=100};
enum{BTN_ID_CW=1,BTN_ID_CC,BTN_ID_SP,BTN_ID_BACK,BTN_ID_LP,S_LAYER_OUT,MESSAGE_KIND_QUICK};
static struct{bool pressed;unsigned id;}btn;
static struct{unsigned index,layer,state,parent_index[20];}ui;
typedef struct{const char *m_select;}M_SELECT;
static char s_draft[129],s_send_text[129],last_notice[64];
static char s_field_edit[129],*s_field_target;
static size_t s_field_capacity;
static draft_selector_t s_field_selector;
static unsigned s_send_kind,s_draft_slot,s_draft_confirm,s_emote_group;
static bool s_draft_loaded,s_box_saving,s_reply;
enum{DRAFT_CONFIRM_OVERWRITE,DRAFT_CONFIRM_DELETE,DRAFT_CONFIRM_NEW,DRAFT_CONFIRM_LOAD};
static draft_selector_t s_draft_selector={.selecting_group=true};
static int selected=-1;
static void ui_navigate_to(unsigned page){assert(ui.layer+1<20);ui.parent_index[++ui.layer]=ui.index;ui.index=page;ui.state=0;}
static void back(void){btn.pressed=false;ui.index=ui.parent_index[ui.layer];ui.state=S_LAYER_OUT;}
static void notice(const char *s){snprintf(last_notice,sizeof(last_notice),"%s",s);}
static void text_line(int y,const char *s){assert(y>=0&&y<=31&&s);}
static void footer(const char *s){assert(s);}
static int menu(M_SELECT *rows,unsigned count){assert(rows&&count);int result=selected;selected=-1;return result;}
'''
production=''.join(function(n) for n in ['load_draft','open_draft_editor','open_content_editor','compose_page','content_edit_page','emotes_page','emote_list_page','draft_box_page','draft_slot_page','draft_confirm_page'])
tests=r'''
static void settle(void){if(ui.state==S_LAYER_OUT){assert(ui.layer);ui.layer--;ui.state=0;}}
static void action(unsigned group){s_draft_selector.group=group;s_draft_selector.selecting_group=true;btn.pressed=true;btn.id=BTN_ID_SP;compose_page();settle();}
int main(void){
 char field[13]="Pet";
 ui.index=96;ui.layer=3;strcpy(s_draft,"Keep my message");strcpy(s_send_text,"No TX");
 open_content_editor(field,sizeof(field));assert(ui.index==100);
 strcpy(s_field_edit,"New name");btn.pressed=true;btn.id=BTN_ID_LP;content_edit_page();assert(ui.index==100&&!strcmp(field,"Pet"));
 btn.pressed=true;btn.id=BTN_ID_SP;content_edit_page();settle();
 assert(ui.index==96&&!strcmp(field,"New name")&&!strcmp(s_draft,"Keep my message")&&!strcmp(s_send_text,"No TX"));
 open_content_editor(field,sizeof(field));strcpy(s_field_edit,"Cancel");btn.pressed=true;btn.id=BTN_ID_BACK;content_edit_page();assert(btn.pressed);back();settle();assert(!strcmp(field,"New name"));
 open_content_editor(s_draft,sizeof(s_draft));strcpy(s_field_edit,"Edited group");fail_commit=true;btn.pressed=true;btn.id=BTN_ID_LP;content_edit_page();btn.pressed=true;btn.id=BTN_ID_SP;content_edit_page();assert(ui.index==100&&!strcmp(s_draft,"Keep my message"));
 fail_commit=false;btn.pressed=true;btn.id=BTN_ID_SP;content_edit_page();settle();assert(ui.index==96&&!strcmp(s_draft,"Edited group")&&!strcmp(s_send_text,"No TX"));
 ui.index=69;open_content_editor(s_draft,sizeof(s_draft));
 s_field_selector.group=DRAFT_EMOTE_GROUP;btn.pressed=true;btn.id=BTN_ID_SP;content_edit_page();assert(ui.index==50);
 selected=0;emotes_page();selected=0;emote_list_page();settle();
 assert(ui.index==100&&!strcmp(s_field_edit,"Edited group(^_^)")&&!strcmp(s_draft,"Edited group")&&!strcmp(s_send_text,"No TX"));
 btn.pressed=true;btn.id=BTN_ID_LP;content_edit_page();btn.pressed=true;btn.id=BTN_ID_SP;content_edit_page();settle();assert(ui.index==69&&!strcmp(s_draft,"Edited group(^_^)")&&!strcmp(s_send_text,"No TX"));
 s_draft[0]=0;s_send_text[0]=0;assert(draft_store_save_work(""));
 ui.index=47;ui.layer=4;ui.parent_index[4]=19;compose_page();assert(!s_draft[0]);
 strcpy(s_draft,"Hi ");action(DRAFT_EMOTE_GROUP);assert(ui.index==50);
 selected=0;emotes_page();assert(ui.index==67);selected=0;emote_list_page();settle();assert(ui.index==47&&!strcmp(s_draft,"Hi (^_^)")&&!s_send_text[0]);
 action(DRAFT_SAVE_GROUP);assert(ui.index==64&&s_box_saving);
 selected=5;draft_box_page();settle();assert(ui.index==47&&!strcmp(draft_store_slot(5),s_draft));
 strcpy(s_draft,"Changed");action(DRAFT_SAVE_GROUP);selected=5;draft_box_page();assert(ui.index==66);
 selected=0;draft_confirm_page();settle();assert(!strcmp(draft_store_slot(5),"Hi (^_^)"));
 selected=5;draft_box_page();selected=1;draft_confirm_page();settle();assert(!strcmp(draft_store_slot(5),"Changed"));
 back();settle();assert(ui.index==47);
 action(DRAFT_BOX_GROUP);selected=5;draft_box_page();assert(ui.index==65);
 strcpy(s_draft,"Do not lose this");selected=0;draft_slot_page();assert(ui.index==66);
 selected=0;draft_confirm_page();settle();assert(!strcmp(s_draft,"Do not lose this"));
 selected=0;draft_slot_page();assert(draft_store_save_work("Old working"));fail_commit=true;selected=1;draft_confirm_page();assert(ui.index==66&&!strcmp(s_draft,"Do not lose this"));
 fail_commit=false;selected=1;draft_confirm_page();settle();assert(ui.index==47&&ui.layer==4&&!strcmp(s_draft,"Changed"));
 for(unsigned i=0;i<30;i++){
  action(DRAFT_BOX_GROUP);selected=5;draft_box_page();selected=0;draft_slot_page();settle();assert(ui.index==47&&ui.layer==4);
 }
 action(DRAFT_NEW_GROUP);selected=0;draft_confirm_page();settle();assert(!strcmp(s_draft,"Changed"));
 action(DRAFT_NEW_GROUP);selected=1;draft_confirm_page();settle();assert(!s_draft[0]&&!draft_store_work()[0]&&!strcmp(draft_store_slot(5),"Changed"));
 strcpy(s_draft,"Hi (^_^)");action(DRAFT_SEND_GROUP);assert(ui.index==29&&!strcmp(s_send_text,s_draft));
 /* Emotes from the hub still send a standalone face, not append to the work. */
 ui.index=50;ui.layer=4;ui.parent_index[4]=19;selected=3;emotes_page();selected=1;emote_list_page();assert(ui.index==29&&!strcmp(s_send_text,"<3")&&!strcmp(s_draft,"Hi (^_^)"));
 ui.index=47;ui.layer=4;memset(s_draft,'A',128);s_draft[128]=0;action(DRAFT_EMOTE_GROUP);selected=0;emotes_page();selected=0;emote_list_page();
 assert(ui.index==67&&strlen(s_draft)==128&&!strcmp(last_notice,"Maximum 128 characters"));
 ui.index=65;selected=1;draft_slot_page();selected=0;draft_confirm_page();settle();assert(draft_store_slot(5)[0]);
 selected=1;draft_slot_page();selected=1;draft_confirm_page();settle();assert(!draft_store_slot(5)[0]);
 ui.index=47;ui.layer=4;s_draft_selector.selecting_group=true;btn.pressed=true;btn.id=BTN_ID_BACK;
 fail_commit=true;compose_page();assert(!btn.pressed&&!strcmp(last_notice,"SAVE ERROR"));
 fail_commit=false;
 for(unsigned g=0;g<EMOTE_GROUPS;g++)for(unsigned r=0;r<8;r++){
  ui.index=47;ui.layer=4;ui.parent_index[4]=19;strcpy(s_draft,"Hi ");action(DRAFT_EMOTE_GROUP);
  selected=(int)g;emotes_page();selected=(int)r;emote_list_page();settle();
  assert(ui.index==47&&ui.layer==4&&!strcmp(s_draft+3,message_emotes[message_emote_group_index(g,r)].text));
  ui.index=50;ui.layer=4;ui.parent_index[4]=19;selected=(int)g;emotes_page();selected=(int)r;emote_list_page();
  assert(ui.index==29&&!strcmp(s_send_text,message_emotes[message_emote_group_index(g,r)].text));
 }
 puts("PASS production draft UI: append vs send, save/edit/delete/clear confirmations, failure retention, recipient copy, 30 ancestor returns");
}
'''
with tempfile.TemporaryDirectory(prefix='halfstep-draft-ui-') as tmp:
 p=Path(tmp);(p/'test.c').write_text(shim+source+ui+production+tests,encoding='utf8')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fno-stack-protector','-I',str(root/'firmware/ui'),str(p/'test.c'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
