/* Included by local_pages.c: shared draft editor, bounded menus, no BLE callbacks. */
static void message_filters_page(void){
    static M_SELECT rows[]={{"- All received"},{"- Unread received"},{"- Current channel"},{"- By friend"},{"- Announcements"}};
    int n=menu(rows,COUNT(rows));if(n<0)return;s_message_filter=(unsigned)n;
    ui_navigate_to(n==3?M_CONVERSATION_PEER:M_FILTERED_INBOX);
}
static void conversation_peer_page(void){
    static M_SELECT rows[FRIEND_STORE_MAX];static char labels[FRIEND_STORE_MAX][32];
    unsigned count=friend_mgr_friend_count();if(count>FRIEND_STORE_MAX)count=FRIEND_STORE_MAX;
    if(!count){text_line(18,"No friends yet");return;}
    for(unsigned i=0;i<count;i++){friend_record_t f;if(!friend_mgr_get_friend(i,&f)){count=i;break;}snprintf(labels[i],32,"- %.13s %02X%02X",f.name,f.mac[4],f.mac[5]);rows[i].m_select=labels[i];}
    int n=menu(rows,count);friend_record_t f;if(n>=0 && friend_mgr_get_friend(n,&f)){memcpy(s_filter_peer,f.mac,6);ui_navigate_to(M_FILTERED_INBOX);}
}
static bool message_matches(const message_inbox_entry_t *e){
    switch(s_message_filter){case 1:return e->unread;case 2:return e->channel==message_service_get_channel();case 3:return mesh_mac_equal(e->src_mac,s_filter_peer);case 4:return !e->channel && e->len>=6 && !memcmp(e->text,"[ALL] ",6);default:return true;}
}
static void filtered_inbox_page(void){
    static M_SELECT rows[MESSAGE_INBOX_MAX];static char labels[MESSAGE_INBOX_MAX][48];static uint32_t ids[MESSAGE_INBOX_MAX];unsigned count=0;
    for(unsigned i=0;i<MESSAGE_INBOX_MAX;i++){message_inbox_entry_t e;if(!message_service_inbox_get(i,&e))break;if(!message_matches(&e))continue;
        char sender[FRIEND_NAME_MAX+1];message_sender_name(e.src_mac,sender);ids[count]=e.local_id;
        snprintf(labels[count],sizeof(labels[count]),"- %c%.13s %.13s",e.unread?'*':' ',sender,e.text);rows[count].m_select=labels[count];count++;
    }
    if(!count){text_line(18,"No matching messages");footer("BACK: FILTERS");return;}
    int n=menu(rows,count);if(n>=0 && message_service_inbox_get_by_id(ids[n],&s_message)){message_service_inbox_mark_read(s_message.local_id);ui_navigate_to(M_MESSAGE_VIEW);}
}
static void message_actions_page(void){
    static M_SELECT rows[]={{"- Back to message"},{"- Reply with draft"},{"- Quote and reply"},{"- Delete this message"}};
    int n=menu(rows,COUNT(rows));if(n==0){back();return;}if(n==3){ui_navigate_to(M_MESSAGE_DELETE);return;}if(n!=1 && n!=2)return;
    bool peer=friend_mgr_is_friend(s_message.src_mac);
    if(!peer && !s_message.channel){notice("Add sender as friend");return;}
    load_draft();char copy[129];snprintf(copy,sizeof(copy),"%s",s_draft);
    if(n==2){
        char snippet[17]={0};for(unsigned i=0;i<16 && i<s_message.len;i++)snippet[i]=(s_message.text[i]>=32 && s_message.text[i]<=126)?s_message.text[i]:'?';
        int len=snprintf(copy,sizeof(copy),">%02X%02X:%s | %s",s_message.src_mac[4],s_message.src_mac[5],snippet,s_draft);
        if(len<0 || len>128){notice("Quote + draft too long");return;}
        if(!draft_store_save_work(copy)){notice("SAVE ERROR");return;}strcpy(s_draft,copy);
    }
    s_reply_channel=peer?0:s_message.channel;memcpy(s_reply_peer,s_message.src_mac,6);s_reply=true;
    s_draft_selector=(draft_selector_t){.selecting_group=true};open_draft_editor();
}
static void reply_confirm_page(void){
    char target[64];snprintf(target,sizeof(target),ui_zh_lookup(s_reply_channel?"- Reply to channel %02u":"- Private reply %02u"),s_reply_channel);
    if(!s_reply_channel)snprintf(target,sizeof(target),ui_zh_lookup("- Private %02X%02X"),s_reply_peer[4],s_reply_peer[5]);
    M_SELECT rows[]={{"- Cancel"},{target},{"- Confirm send reply"}};int n=menu(rows,COUNT(rows));if(n==0){back();return;}if(n!=2)return;
    if(!s_reply || (s_reply_channel && s_reply_channel!=message_service_get_channel())){notice("Return to source channel");return;}
    if(!s_reply_channel && !friend_mgr_is_friend(s_reply_peer)){notice("Friend no longer exists");return;}
    uint8_t broadcast[6]=MESH_DST_BROADCAST_MAC;
    if(!message_service_send_text(MESSAGE_KIND_QUICK,s_reply_channel?broadcast:s_reply_peer,s_reply_channel,s_send_text,(uint16_t)strlen(s_send_text),!s_reply_channel,&s_tx_id)){notice("Busy / send rejected");return;}
    s_reply=false;ui_navigate_to(M_TX_STATUS);
}
static void message_delete_page(void){
    static M_SELECT rows[]={{"- Keep message"},{"- Confirm delete message"}};int n=menu(rows,COUNT(rows));if(n==0)back();if(n==1){
        if(!message_service_inbox_remove(s_message.local_id)){notice("DELETE ERROR");return;}
        /* Return to the existing list ancestor; never retain a deleted message view. */
        for(unsigned layer=ui.layer;layer>0;layer--)if(ui.parent_index[layer]==M_INBOX || ui.parent_index[layer]==M_FILTERED_INBOX){ui.index=ui.parent_index[layer];ui.layer=layer;ui.state=S_LAYER_OUT;return;}
        back();
    }
}
static void contact_status_page(void){
    contact_presence_t p;message_service_contact(s_friend.mac,&p);char line[64];
    text_line(9,p.state==CONTACT_DIRECT?"Directly heard <15s":p.state==CONTACT_RECENT?"Recently heard <60s":"Status unknown");
    if(p.state==CONTACT_DIRECT)snprintf(line,sizeof(line),ui_zh_lookup("Direct RSSI %d dBm"),p.rssi);
    else if(p.state==CONTACT_RECENT)snprintf(line,sizeof(line),ui_zh_lookup("%lus ago / %u hops"),(unsigned long)(p.age_ms/1000),p.hops);
    else snprintf(line,sizeof(line),"Enable mesh to discover");
    text_line(21,line);footer("Observed, not guaranteed");
}
static void team_result(const message_team_view_t *v){
    if(!v->enabled){footer("Enable mesh in Settings");return;}
    static const char *states[]={"RAM only / same channel","Waiting for host ACK","Host confirmed","Rejected / refresh view","No ACK: check host view"};
    footer(states[v->result<=TEAM_TIMEOUT?v->result:0]);
}
static void poll_page(void){
    message_team_view_t v;message_service_team_view(&v);
    static M_SELECT rows[9];static char labels[9][64];unsigned count=0;
    if(v.poll.active){
        snprintf(labels[count],64,"- %s %.48s",ui_zh_lookup(v.poll.open?"POLL":"CLOSED"),v.poll.question);rows[count].m_select=labels[count];count++;
        for(unsigned i=0;i<v.poll.options;i++){snprintf(labels[count],64,ui_zh_lookup("- Vote %c: %u"),'A'+i,v.poll.counts[i]);rows[count].m_select=labels[count];count++;}
        snprintf(labels[count],64,"- %s %u/8",ui_zh_lookup(v.poll.local?"Host / voters":((uint32_t)(esp_timer_get_time()/1000)-v.poll.seen>=15000?"Host stale / voters":"Remote / voters")),v.poll.voters);rows[count].m_select=labels[count];count++;
        if(v.poll.local){rows[count++].m_select="- Close voting";}
    }
    unsigned create=count;rows[count++].m_select="- Create poll";
    int n=menu(rows,count);team_result(&v);if(n<0)return;
    if((unsigned)n==create){ui_navigate_to(M_POLL_CREATE);return;}
    if(n>0 && (unsigned)n<=v.poll.options){if(!message_service_team_action(MESSAGE_POLL_VOTE,NULL,n-1,0))notice("Vote rejected / busy");}
    else if(v.poll.local && (unsigned)n==create-1){s_team_action=MESSAGE_POLL_CLOSE;ui_navigate_to(M_TEAM_CONFIRM);}
}
static void team_create_page(bool poll){
    char preview[96],options[64];snprintf(preview,sizeof(preview),ui_zh_lookup("- Edit: %.48s"),s_form_text[0]?s_form_text:ui_zh_lookup("[EMPTY]"));snprintf(options,sizeof(options),ui_zh_lookup("- Options: %u (A-D)"),s_team_options);
    M_SELECT rows[]={{preview},{poll?options:"- Title: max 18 ASCII"},{poll?"- Preview new poll":"- Preview new board"},{"- Add task to my board"}};
    int n=menu(rows,poll?3:4);if(n==0){open_content_editor(s_form_text,poll?49:19);return;}if(n==1 && poll)s_team_options=s_team_options==4?2:s_team_options+1;
    if(n!=2 && !(n==3 && !poll))return;
    size_t len=strlen(s_form_text);if(!len || len>(poll?48u:18u)){notice(poll?"Need 1-48 ASCII chars":"Need 1-18 ASCII chars");return;}
    for(unsigned i=0;i<len;i++)if(s_form_text[i]<32 || s_form_text[i]>126){notice("ASCII title required");return;}
    strcpy(s_team_text,s_form_text);s_team_action=poll?MESSAGE_POLL_CREATE:n==3?MESSAGE_TASK_ADD:MESSAGE_BOARD_CREATE;ui_navigate_to(M_TEAM_CONFIRM);
}
static void team_confirm_page(void){
    char preview[96];snprintf(preview,sizeof(preview),"- %.48s",s_team_action==MESSAGE_POLL_CLOSE?ui_zh_lookup("Close poll?"):s_team_text);
    M_SELECT rows[]={{"- Cancel"},{preview},{"- Confirm activity change"}};int n=menu(rows,COUNT(rows));if(n==0){back();return;}if(n!=2)return;
    if(!message_service_team_action(s_team_action,s_team_text,0,s_team_options)){notice("Mesh off / host busy");return;}notice("Activity updated");back();
}
static void task_board_page(void){
    message_team_view_t v;message_service_team_view(&v);static M_SELECT rows[6];static char labels[6][64];unsigned count=0;
    if(v.board.active){rows[count++].m_select=v.board.local?"- My board (RAM only)":((uint32_t)(esp_timer_get_time()/1000)-v.board.seen>=15000?"- Host stale / old board":"- Remote board (RAM only)");
        for(unsigned i=0;i<4;i++){team_task_t *t=&v.board.tasks[i];snprintf(labels[i],64,"- %u [%s] %.18s",i+1,ui_zh_lookup(t->state==3?"DONE":t->state==2?"TAKEN":t->state==1?"OPEN":"EMPTY"),t->title);rows[count++].m_select=labels[i];}}
    unsigned create=count;rows[count++].m_select="- Create / add task";int n=menu(rows,count);team_result(&v);
    if(n>=0 && (unsigned)n==create){ui_navigate_to(M_TASK_CREATE);return;}
    if(n>=1 && n<=4 && v.board.active){s_team_task=n-1;ui_navigate_to(M_TASK_DETAIL);}
}
static void task_detail_page(void){
    message_team_view_t v;message_service_team_view(&v);if(!v.board.active || s_team_task>=4){text_line(18,"Board ended");return;}
    team_task_t *t=&v.board.tasks[s_team_task];char title[64],owner[64],name[FRIEND_NAME_MAX+1];message_sender_name(t->owner,name);
    snprintf(title,sizeof(title),ui_zh_lookup("- Title: %.18s"),t->title);snprintf(owner,sizeof(owner),ui_zh_lookup("- Owner: %.13s"),t->state>=2?name:ui_zh_lookup("none"));
    M_SELECT rows[]={{title},{owner},{"- Claim task"},{"- Mark completed"},{"- Release task"}};int n=menu(rows,COUNT(rows));team_result(&v);
    if(n>=2 && n<=4 && !message_service_team_action(MESSAGE_TASK_ACTION,NULL,s_team_task,n-1))notice("Task rejected / busy");
}
