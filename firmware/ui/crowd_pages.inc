/* Shared editor and list ownership; all radio/storage calls use service locks. */
static void crowd_result(const message_crowd_view_t *v){
    static const char *states[]={"RAM only / same channel","Waiting for host ACK","Host confirmed","Rejected / refresh view","No ACK: check host view"};
    footer(v->enabled?states[v->result<=TEAM_TIMEOUT?v->result:0]:"Enable mesh in Settings");
}
static const char *checkin_state(unsigned status){static const char *states[]={"Unknown","Arrived","On the way","Not joining"};return ui_zh_lookup(states[status<=3?status:0]);}
static void crowd_page(unsigned kind){
    s_crowd_kind=kind;message_crowd_view_t v;message_service_crowd_view(&v);crowd_room_t *r=&v.rooms[kind];
    static M_SELECT rows[16];static char labels[16][96];unsigned count=0,request_at=99,close_at=99;
    if(r->active){
        snprintf(labels[count],96,ui_zh_lookup("- Topic: %.32s"),r->title);rows[count].m_select=labels[count];count++;
        bool stale=!r->local && (uint32_t)(esp_timer_get_time()/1000)-r->seen>=15000;
        rows[count++].m_select=stale?"- Host not heard recently":r->open?(kind==CROWD_CHECKIN?"- Check-in open":"- Buzzer OPEN"):"- Activity closed";
        if(kind==CROWD_CHECKIN){
            request_at=count;rows[count++].m_select="- I have arrived";rows[count++].m_select="- I am on the way";rows[count++].m_select="- I will not join";
            for(unsigned i=0;i<r->count;i++){char name[FRIEND_NAME_MAX+1];message_sender_name(r->people[i].mac,name);snprintf(labels[count],96,"- %.13s: %s",name,checkin_state(r->people[i].status));rows[count].m_select=labels[count];count++;}
        }else{
            request_at=count;rows[count++].m_select="- BUZZ NOW";
            char name[FRIEND_NAME_MAX+1];message_sender_name(r->winner,name);
            snprintf(labels[count],96,ui_zh_lookup("- Winner: %.13s"),mesh_mac_is_valid_peer(r->winner)?name:ui_zh_lookup("none"));rows[count].m_select=labels[count];count++;
        }
        if(r->local && r->open){close_at=count;rows[count++].m_select=kind==CROWD_CHECKIN?"- Close check-in":"- Cancel buzzer round";}
    }
    unsigned create=count;rows[count++].m_select=kind==CROWD_CHECKIN?"- Create check-in":"- Open a new buzzer round";
    int n=menu(rows,count);crowd_result(&v);if(n<0)return;
    if((unsigned)n==create){ui_navigate_to(M_CROWD_CREATE);return;}
    if((unsigned)n==close_at){s_crowd_action=MESSAGE_CROWD_CLOSE;ui_navigate_to(M_CROWD_CONFIRM);return;}
    if((unsigned)n>=request_at && (unsigned)n<request_at+(kind==CROWD_CHECKIN?3u:1u)){
        if(!message_service_crowd_action(MESSAGE_CROWD_STATUS,kind,NULL,(unsigned)n-request_at+1))notice("Request rejected / busy");
    }
}
static void crowd_create_page(void){
    char preview[96];snprintf(preview,sizeof(preview),ui_zh_lookup("- Edit: %.32s"),s_form_text[0]?s_form_text:ui_zh_lookup("[EMPTY]"));
    M_SELECT rows[]={{preview},{"- Title: max 32 ASCII"},{s_crowd_kind==CROWD_CHECKIN?"- Preview check-in":"- Preview buzzer round"}};
    int n=menu(rows,COUNT(rows));if(n==0){open_content_editor(s_form_text,33);return;}if(n!=2)return;
    unsigned len=strlen(s_form_text);if(!len || len>32){notice("Need 1-32 ASCII chars");return;}
    for(unsigned i=0;i<len;i++)if(s_form_text[i]<32 || s_form_text[i]>126){notice("ASCII title required");return;}
    strcpy(s_crowd_title,s_form_text);s_crowd_action=MESSAGE_CROWD_CREATE;ui_navigate_to(M_CROWD_CONFIRM);
}
static void crowd_confirm_page(void){
    char title[96];snprintf(title,sizeof(title),ui_zh_lookup("- Topic: %.32s"),s_crowd_action==MESSAGE_CROWD_CREATE?s_crowd_title:ui_zh_lookup("Close this activity?"));
    M_SELECT rows[]={{"- Cancel"},{title},{"- Confirm activity change"}};int n=menu(rows,COUNT(rows));if(n==0){back();return;}if(n!=2)return;
    if(!message_service_crowd_action(s_crowd_action,s_crowd_kind,s_crowd_title,0)){notice("Mesh off / host busy");return;}notice("Activity updated");back();
}
static void network_diagnostics_page(void){
    message_network_view_t net;message_service_network_view(&net);message_service_snapshot_t stats;message_service_snapshot(&stats);message_diagnostics_t diag;message_service_diagnostics(&diag);
    static M_SELECT rows[11];static char labels[11][96];unsigned peers=0;for(unsigned i=0;i<ROUTE_PEERS;i++)if(net.peers[i].used && net.now-net.peers[i].seen<15000)peers++;
    snprintf(labels[0],96,ui_zh_lookup("- CH%u direct:%u heard:%u hops"),message_service_get_channel(),peers,net.last_rx_hops);
    snprintf(labels[1],96,ui_zh_lookup("- Forwarded:%lu dup:%lu"),(unsigned long)net.forwarded,(unsigned long)net.duplicates);
    snprintf(labels[2],96,ui_zh_lookup("- Route drop:%lu RX queue:%lu"),(unsigned long)net.dropped,(unsigned long)stats.rx_queue_dropped);
    snprintf(labels[3],96,ui_zh_lookup("- Text sent:%lu receipts:%lu"),(unsigned long)stats.tx_sent,(unsigned long)stats.tx_delivered);
    snprintf(labels[4],96,ui_zh_lookup("- Accepted:%lu rejected:%lu"),(unsigned long)stats.rx_accepted,(unsigned long)stats.rx_rejected);
    snprintf(labels[5],96,ui_zh_lookup("- Poll/task ACK:%lu ms"),(unsigned long)diag.team_ack_ms);
    snprintf(labels[6],96,ui_zh_lookup("- Check/buzz ACK:%lu ms"),(unsigned long)diag.crowd_ack_ms);
    snprintf(labels[7],96,ui_zh_lookup("- ACKs:%lu retries:%lu"),(unsigned long)diag.confirmed,(unsigned long)diag.retries);
    snprintf(labels[8],96,ui_zh_lookup("- Timeouts:%lu records:%u"),(unsigned long)diag.timeouts,diag.records);
    for(unsigned i=0;i<9;i++){rows[i].m_select=labels[i];}
    rows[9].m_select=diag.log_error?"- History SAVE ERROR":"- Activity history";rows[10].m_select="- Network settings";
    int n=menu(rows,COUNT(rows));if(n==9)ui_navigate_to(M_ACTIVITY_HISTORY);if(n==10)ui_navigate_to(M_NETWORK_STATUS);
    footer(net.paused?"MESSAGING PAUSED":"Counters reset on reboot");
}
static const char *activity_kind(unsigned kind){static const char *names[]={"Unknown","Poll ended","Task completed","Check-in ended","Buzzer ended"};return ui_zh_lookup(names[kind<=4?kind:0]);}
static void activity_history_page(void){
    message_diagnostics_t d;message_service_diagnostics(&d);if(!d.records){text_line(10,d.log_error?"History SAVE ERROR":"No activity records");text_line(24,"Saved after activity ends");return;}
    static M_SELECT rows[ACTIVITY_LOG_MAX];static char labels[ACTIVITY_LOG_MAX][128];unsigned count=0;
    for(unsigned i=0;i<ACTIVITY_LOG_MAX;i++){activity_record_t r;if(!message_service_activity_get(i,&r))break;snprintf(labels[i],128,"- %lu %s %.48s",(unsigned long)r.sequence,activity_kind(r.kind),r.title);rows[i].m_select=labels[i];count++;}
    int n=menu(rows,count);if(n>=0 && message_service_activity_get(n,&s_activity_detail)){s_activity_index=n;ui_navigate_to(M_ACTIVITY_DETAIL);}if(d.log_error)footer("History SAVE ERROR");
}
static void activity_detail_page(void){
    activity_record_t r=s_activity_detail;if(!r.kind){text_line(18,"Record not available");return;}
    static M_SELECT rows[6];static char labels[6][128];char peer[FRIEND_NAME_MAX+1];message_sender_name(r.peer,peer);
    snprintf(labels[0],128,"- %s",activity_kind(r.kind));snprintf(labels[1],128,ui_zh_lookup("- Title: %.48s"),r.title);
    if(r.kind==ACTIVITY_POLL)snprintf(labels[2],128,"- A:%u B:%u C:%u D:%u",r.values[0],r.values[1],r.values[2],r.values[3]);
    else if(r.kind==ACTIVITY_CHECKIN)snprintf(labels[2],128,ui_zh_lookup("- Arrived:%u way:%u no:%u"),r.values[0],r.values[1],r.values[2]);
    else snprintf(labels[2],128,ui_zh_lookup("- Peer: %.13s"),mesh_mac_is_valid_peer(r.peer)?peer:ui_zh_lookup("none"));
    snprintf(labels[3],128,ui_zh_lookup("- Channel:%u sequence:%lu"),r.channel,(unsigned long)r.sequence);
    snprintf(labels[4],128,ui_zh_lookup("- Saved at uptime:%lu s"),(unsigned long)r.uptime_s);
    snprintf(labels[5],128,ui_zh_lookup("- Host:%02X%02X part:%u"),r.host[4],r.host[5],r.part+1);
    for(unsigned i=0;i<6;i++){rows[i].m_select=labels[i];}
    menu(rows,COUNT(rows));
}
