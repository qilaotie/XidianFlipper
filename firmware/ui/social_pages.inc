enum {SOC_ROOM_CREATE,SOC_ROOM_JOIN,SOC_EDIT_NICK,SOC_EDIT_SIGN,SOC_SYNC_PET,SOC_DROP_RELAY};
static void social_form_open(unsigned action){s_social_action=action;s_form_text[0]=0;social_view_t v;message_service_social_view(&v);if(action==SOC_EDIT_NICK)snprintf(s_form_text,sizeof(s_form_text),"%s",v.nickname);if(action==SOC_EDIT_SIGN)snprintf(s_form_text,sizeof(s_form_text),"%s",v.signature);ui_navigate_to(M_SOCIAL_FORM);}
static void social_hub_page(void){
    static social_view_t v;message_service_social_view(&v);
    if(s_social_section && btn.pressed && btn.id==BTN_ID_BACK){btn.pressed=false;s_social_section=0;ui.select[ui.layer]=0;ui.init=false;s_draft_list_enter=true;return;}
    if(!s_social_section){static M_SELECT rows[]={{"- Team / room"},{"- Relay mail cache"},{"- Personal profile cards"},{"- Pet travel passport"},{"- Light and sound ensemble"}};int n=menu(rows,COUNT(rows));if(n>=0 && n<3){s_social_section=n+1;ui.select[ui.layer]=0;ui.init=false;s_draft_list_enter=true;}if(n==3)ui_navigate_to(M_PASSPORT);if(n==4)ui_navigate_to(M_ENSEMBLE);return;}
    if(s_social_section==1){
        char code[96],name[96];snprintf(code,sizeof(code),ui_zh_lookup("- %08lX %s CH%u"),(unsigned long)v.room,ui_zh_lookup(v.active?"ON":"OFF"),message_service_get_channel());snprintf(name,sizeof(name),ui_zh_lookup("- Title: %.18s"),v.room_name);
        static M_SELECT rows[15];static char labels[8][96];rows[0].m_select=code;rows[1].m_select=name;rows[2].m_select="- Create room";rows[3].m_select="- Join 8-hex code";rows[4].m_select="- Leave room";rows[5].m_select="- Resume saved room";rows[6].m_select="- Room group chat";unsigned count=7;
        for(unsigned i=0;i<8;i++)if(v.peers[i].used && (uint32_t)(esp_timer_get_time()/1000)-v.peers[i].seen<30000){snprintf(labels[i],96,"- %.12s %02X%02X",v.peers[i].nickname,v.peers[i].mac[4],v.peers[i].mac[5]);rows[count++].m_select=labels[i];}
        int n=menu(rows,count);if(n==2)social_form_open(SOC_ROOM_CREATE);if(n==3)social_form_open(SOC_ROOM_JOIN);
        if(n==4 && !message_service_room(0,""))notice("SAVE ERROR");
        if(n==5 && (!v.room || !message_service_room(v.room,v.room_name)))notice("SAVE ERROR");
        if(n==6){s_net_mode=NET_GROUP;ui_navigate_to(M_NET_SEND);}footer("Room code is not encryption");return;
    }
    if(s_social_section==2){
        relay_mail_view_t state;message_service_relay_view(&state);char status[96];snprintf(status,sizeof(status),ui_zh_lookup("- Relay cache %s %u/4"),ui_zh_lookup(state.enabled?"ON":"OFF"),state.count);
        static char labels[4][96];M_SELECT rows[7]={{status},{"- Send relay mail from draft"},{"- Plaintext / 30 active minutes"}};
        for(unsigned i=0;i<4;i++){relay_mail_entry_t e;message_service_relay_get(i,&e);snprintf(labels[i],96,ui_zh_lookup("- Slot%u %umin tries%u"),i+1,e.minutes,e.attempts);rows[3+i].m_select=labels[i];}
        int n=menu(rows,COUNT(rows));if(n==0)message_service_relay_enable(!state.enabled);if(n==1){s_net_mode=NET_CUSTODY;ui_navigate_to(M_NET_SEND);}if(n>=3){s_relay_slot=n-3;social_form_open(SOC_DROP_RELAY);}if(state.error)footer("SAVE ERROR");return;
    }
    static M_SELECT rows[13];static char labels[8][96];rows[0].m_select="- My profile card";rows[1].m_select="- Edit nickname";rows[2].m_select="- Edit signature";rows[3].m_select="- Copy my current pet name";rows[4].m_select=v.sharing?"- Share card: ON":"- Share card: OFF";
    unsigned count=5,map[8];for(unsigned i=0;i<8;i++)if(v.peers[i].used && (uint32_t)(esp_timer_get_time()/1000)-v.peers[i].seen<60000){map[count-5]=i;snprintf(labels[i],96,"- %.12s %02X%02X",v.peers[i].nickname,v.peers[i].mac[4],v.peers[i].mac[5]);rows[count++].m_select=labels[i];}
    int n=menu(rows,count);if(n==0){s_card_peer=8;ui_navigate_to(M_PROFILE_CARD);}if(n==1)social_form_open(SOC_EDIT_NICK);if(n==2)social_form_open(SOC_EDIT_SIGN);if(n==3)social_form_open(SOC_SYNC_PET);if(n==4 && !message_service_card_share(!v.sharing))notice("SAVE ERROR");if(n>=5){s_card_peer=map[n-5];ui_navigate_to(M_PROFILE_CARD);}
}
static void social_form_page(void){
    char preview[128],cached[49];const char *text=s_social_action==SOC_SYNC_PET?pet_service_name():s_form_text;
    if(s_social_action==SOC_DROP_RELAY){relay_mail_entry_t e;if(!message_service_relay_get(s_relay_slot,&e)){notice("SAVE ERROR");return;}unsigned n=e.packet.len<48?e.packet.len:48;memcpy(cached,e.packet.payload,n);cached[n]=0;text=cached;}
    snprintf(preview,sizeof(preview),ui_zh_lookup("- Title: %.48s"),text);
    M_SELECT rows[]={{"- Cancel"},{preview},{"- Edit content"},{"- Confirm activity change"}};int n=menu(rows,COUNT(rows));if(n==0){back();return;}if(n==2 && s_social_action<=SOC_EDIT_SIGN){open_content_editor(s_form_text,s_social_action==SOC_ROOM_JOIN?9:s_social_action==SOC_EDIT_NICK?13:s_social_action==SOC_EDIT_SIGN?25:19);return;}if(n!=3)return;bool ok=false;
    if(s_social_action==SOC_ROOM_CREATE){uint32_t code=esp_random();if(!code)code=1;ok=message_service_room(code,s_form_text);}
    if(s_social_action==SOC_ROOM_JOIN){uint32_t code=0;bool valid=strlen(s_form_text)==8;for(unsigned i=0;valid && i<8;i++){unsigned c=(uint8_t)s_form_text[i];if(c>='0' && c<='9')c-='0';else if(c>='a' && c<='f')c=c-'a'+10;else if(c>='A' && c<='F')c=c-'A'+10;else{valid=false;break;}code=(code<<4)|c;}if(valid && code)ok=message_service_room(code,"Joined room");}
    if(s_social_action==SOC_EDIT_NICK)ok=message_service_card_edit(0,s_form_text);
    if(s_social_action==SOC_EDIT_SIGN)ok=message_service_card_edit(1,s_form_text);
    if(s_social_action==SOC_SYNC_PET)ok=message_service_card_edit(2,text);
    if(s_social_action==SOC_DROP_RELAY)ok=message_service_relay_remove(s_relay_slot);
    if(ok){notice("Activity updated");back();}else notice("Invalid length/code or SAVE ERROR");
}
static void profile_card_page(void){
    static social_view_t v;message_service_social_view(&v);char a[96],b[96],c[96];const char *nick=v.nickname,*sign=v.signature,*pet=v.pet;
    if(s_card_peer<8){if(!v.peers[s_card_peer].used || (uint32_t)(esp_timer_get_time()/1000)-v.peers[s_card_peer].seen>=60000){text_line(18,"Record not available");return;}nick=v.peers[s_card_peer].nickname;sign=v.peers[s_card_peer].signature;pet=v.peers[s_card_peer].pet;}
    snprintf(a,sizeof(a),ui_zh_lookup("- Nickname: %.12s"),*nick?nick:(s_card_peer==8?nvs_config_get_device_name():ui_zh_lookup("Unknown")));snprintf(b,sizeof(b),ui_zh_lookup("- Signature: %.24s"),sign);snprintf(c,sizeof(c),ui_zh_lookup("- Pet name: %.12s"),pet);
    M_SELECT rows[]={{a},{b},{c},{s_card_peer==8?(v.sharing?"- Share card: ON":"- Share card: OFF"):"- Received card / may expire"}};menu(rows,COUNT(rows));
}
static void passport_page(void){
    static social_view_t v;message_service_social_view(&v);if(!v.stamps_count){text_line(10,"No travel stamps yet");text_line(24,"Finish a pet visit to earn one");return;}
    if(s_stamp_detail){passport_stamp_t *s=&v.stamps[s_stamp_slot];char a[96],b[96],c[96];snprintf(a,sizeof(a),ui_zh_lookup("- Pet name: %.12s"),s->pet);snprintf(b,sizeof(b),ui_zh_lookup("- Visits: %lu"),(unsigned long)s->visits);snprintf(c,sizeof(c),"- %02X:%02X:%02X:%02X:%02X:%02X",s->mac[0],s->mac[1],s->mac[2],s->mac[3],s->mac[4],s->mac[5]);M_SELECT rows[]={{a},{b},{c}};menu(rows,3);if(btn.pressed && btn.id==BTN_ID_BACK){btn.pressed=false;s_stamp_detail=false;ui.select[ui.layer]=0;s_draft_list_enter=true;}return;}
    static M_SELECT rows[12];static char labels[12][96];for(unsigned i=0;i<v.stamps_count;i++){snprintf(labels[i],96,"- %u %.12s %02X%02X",i+1,v.stamps[i].pet,v.stamps[i].mac[4],v.stamps[i].mac[5]);rows[i].m_select=labels[i];}int n=menu(rows,v.stamps_count);if(n>=0){s_stamp_slot=n;s_stamp_detail=true;ui.select[ui.layer]=0;s_draft_list_enter=true;}
}
static void ensemble_page(void){
    unsigned note,role,beat;if(message_service_ensemble_event(&s_music_sequence,&note,&role,&beat)){
        s_music_last_beat=beat+1;
        if(role<3 && (!buzzer_is_playing() || buzzer_sequence_is_active(s_music_token))){buzzer_note_t sound={.frequency_hz=role==0?2000+note*200:role==1?2000+note*80:4500,.duration_ms=role==2?40:110};buzzer_sequence_start(&sound,1,&s_music_token);}
        rgb_led_trigger_color(RGB_EVENT_MSG_FORWARD,(beat%3==0)?70:5,(beat%3==1)?70:5,(beat%3==2)?70:5);
    }
    static const char *roles[]={"Melody","Bass","Beat","Light only"};
    char role_label[96],tempo[96],tune[96];snprintf(role_label,sizeof(role_label),ui_zh_lookup("- Role %u: %s"),s_music_role+1,ui_zh_lookup(roles[s_music_role]));snprintf(tempo,sizeof(tempo),"- BPM: %u",s_music_bpm);snprintf(tune,sizeof(tune),ui_zh_lookup("- Tune: %u"),s_music_tune+1);
    char progress[96];snprintf(progress,sizeof(progress),ui_zh_lookup("- Listening / beat %u of 16"),s_music_last_beat);
    M_SELECT rows[]={{progress},{role_label},{tempo},{tune},{"- Start 16-beat ensemble"},{"- Re-arm listening (60s)"}};int n=menu(rows,COUNT(rows));if(n==1){s_music_role=(s_music_role+1)%4;message_service_ensemble_arm(true,s_music_role);}if(n==2)s_music_bpm=s_music_bpm==150?90:s_music_bpm+30;if(n==3)s_music_tune=(s_music_tune+1)%3;if(n==4 && !message_service_ensemble_start(s_music_tune,s_music_bpm))notice("Enable mesh / busy");if(n==5)message_service_ensemble_arm(true,s_music_role);footer("Mute/LED settings apply; approximate sync");
}
