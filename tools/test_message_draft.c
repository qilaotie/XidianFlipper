#include "message_draft.h"
#include "message_emotes.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    draft_selector_t selector={.selecting_group=true};
    assert(!message_draft_group_back(&selector));
    message_draft_group_rotate(&selector,-1);assert(selector.group==DRAFT_EMOTE_GROUP);
    message_draft_group_rotate(&selector,1);assert(selector.group==DRAFT_UPPER);
    bool covered[95]={false};
    for(unsigned group=0;group<DRAFT_GROUPS;group++) {
        selector.group=group;selector.selecting_group=false;
        unsigned count=message_draft_group_count(group);
        for(unsigned i=0;i<count;i++) {
            selector.position[group]=i;
            unsigned choice=message_draft_group_choice(&selector);
            assert(choice<=DRAFT_EMOTE);
            if(choice<95)covered[choice]=true;
        }
        selector.position[group]=0;message_draft_group_rotate(&selector,-1);
        assert(selector.position[group]==count-1);
        message_draft_group_rotate(&selector,1);assert(selector.position[group]==0);
    }
    for(unsigned i=0;i<95;i++)assert(covered[i]);
    selector.group=DRAFT_LOWER;selector.position[DRAFT_LOWER]=7;
    assert(message_draft_group_back(&selector));
    message_draft_group_rotate(&selector,1);message_draft_group_rotate(&selector,-1);
    selector.selecting_group=false;assert(message_draft_group_choice(&selector)=='h'-32);
    message_draft_select_send(&selector);
    assert(selector.selecting_group && message_draft_group_choice(&selector)==DRAFT_SEND);
    selector.group=DRAFT_SPACE_GROUP;assert(message_draft_group_choice(&selector)==0);
    selector.group=DRAFT_DELETE_GROUP;assert(message_draft_group_choice(&selector)==DRAFT_DELETE);
    char send_buffer[129]="old",untouched_draft[129]="My draft";
    for(unsigned i=0;i<MESSAGE_EMOTE_COUNT;i++) {
        assert(message_emote_copy(i,send_buffer,sizeof(send_buffer)));
        assert(!strcmp(send_buffer,message_emotes[i].text));
        for(unsigned j=0;send_buffer[j];j++)assert(send_buffer[j]>=32 && send_buffer[j]<=126);
    }
    assert(!strcmp(untouched_draft,"My draft"));
    assert(!message_emote_copy(99,send_buffer,sizeof(send_buffer)));
    assert(!message_emote_copy(0,send_buffer,2));
    struct { char text[129]; char guard; } draft={.guard=42};
    assert(message_draft_rotate(0,-1)==DRAFT_SEND);
    assert(message_draft_rotate(DRAFT_SEND,1)==0);
    assert(message_draft_edit(draft.text,sizeof(draft.text),'H'-32));
    assert(message_draft_edit(draft.text,sizeof(draft.text),'i'-32));
    assert(message_draft_edit(draft.text,sizeof(draft.text),0));
    assert(message_draft_edit(draft.text,sizeof(draft.text),'!'-32));
    assert(!strcmp(draft.text,"Hi !"));
    assert(message_draft_edit(draft.text,sizeof(draft.text),DRAFT_DELETE));
    assert(!strcmp(draft.text,"Hi "));
    assert(!message_draft_edit(draft.text,sizeof(draft.text),DRAFT_SEND));
    draft.text[0]=0;
    assert(message_draft_edit(draft.text,sizeof(draft.text),DRAFT_DELETE));
    for(unsigned i=0;i<128;i++) assert(message_draft_edit(draft.text,sizeof(draft.text),'A'-32));
    assert(strlen(draft.text)==128 && draft.guard==42);
    assert(!message_draft_edit(draft.text,sizeof(draft.text),'B'-32));
    assert(strlen(draft.text)==128 && draft.guard==42);
    assert(message_draft_edit(draft.text,sizeof(draft.text),DRAFT_DELETE));
    assert(message_draft_edit(draft.text,sizeof(draft.text),'Z'-32));
    assert(draft.text[127]=='Z' && draft.guard==42);
    for(unsigned i=0;i<95;i++) {
        draft.text[0]=0;
        assert(message_draft_edit(draft.text,sizeof(draft.text),i));
        assert(draft.text[0]==(char)(32+i) && draft.text[1]==0);
    }
    puts("PASS: printable ASCII, delete, empty draft, 128-char bound, selector wrap");
}
