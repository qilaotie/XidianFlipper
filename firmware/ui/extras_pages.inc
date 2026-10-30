/* Explicit peer selection and immutable gift confirmation, UI owner only. */
static route_peer_t lab_peers[ROUTE_PEERS];static unsigned lab_count,lab_preset;
static uint8_t lab_target[6];static char lab_name[14];
static gift_peer_t gift_peers[6];static unsigned gift_count,gift_choice,gift_confirm;
static uint8_t gift_target[6];static char gift_name[13];static gift_tx_t gift_preview;
static M_SELECT extra_rows[ROUTE_PEERS+8];static char extra_labels[ROUTE_PEERS+8][128];
static void lab_refresh(void){static message_network_view_t net;message_service_network_view(&net);lab_count=0;for(unsigned i=0;i<ROUTE_PEERS;i++)if(net.peers[i].used && net.now-net.peers[i].seen<15000)lab_peers[lab_count++]=net.peers[i];}
static void gift_refresh(void){pet_gifts_t *s=gift_runtime_state();gift_count=0;for(unsigned i=0;i<6;i++)if(s->peers[i].used && s->now-s->peers[i].seen<15000)gift_peers[gift_count++]=s->peers[i];}
static void extras_pages_enter(unsigned page){if(page==M_NETWORK_LAB){lab_refresh();s_draft_list_enter=true;}if(page==M_PET_GIFTS){gift_confirm=0;gift_refresh();s_draft_list_enter=true;}}
static void lab_page(void){
    network_lab_view_t v;message_service_lab_view(&v);M_SELECT *rows=extra_rows;char (*label)[128]=extra_labels;
    snprintf(label[0],128,"%s",ui_zh_lookup(v.local_error?"- Local send queue blocked":v.enabled?"- Network settings":"- Enable network first"));
    static const char *presets[]={"10 probes / 0 retries","10 probes / 2 retries","30 probes / 1 retry"};snprintf(label[1],128,"- %s",ui_zh_lookup(presets[lab_preset]));
    snprintf(label[2],128,ui_zh_lookup("- Target: %.13s [%02X%02X]"),lab_name[0]?lab_name:"?",lab_target[4],lab_target[5]);
    snprintf(label[3],128,ui_zh_lookup("- Last [%02X%02X] ACK:%lu/%lu"),v.target[4],v.target[5],(unsigned long)v.confirmed,(unsigned long)v.started);
    snprintf(label[4],128,ui_zh_lookup("- Try:%lu retry:%lu timeout:%lu"),(unsigned long)v.attempts,(unsigned long)v.retries,(unsigned long)v.timeouts);
    if(v.confirmed)snprintf(label[5],128,ui_zh_lookup("- RTT min/avg/max %lu/%lu/%lu ms"),(unsigned long)v.rtt_min,(unsigned long)(v.rtt_sum/v.confirmed),(unsigned long)v.rtt_max);else snprintf(label[5],128,"%s",ui_zh_lookup("- RTT: no confirmed reply"));
    snprintf(label[6],128,"%s",ui_zh_lookup(v.running?"- Stop experiment":"- Start experiment"));snprintf(label[7],128,"%s",ui_zh_lookup("- Refresh direct neighbors"));
    for(unsigned i=0;i<lab_count;i++)snprintf(label[i+8],128,"- %.13s [%02X%02X]",lab_peers[i].name,lab_peers[i].mac[4],lab_peers[i].mac[5]);
    for(unsigned i=0;i<lab_count+8;i++){rows[i].m_select=label[i];}
    int n=menu(rows,lab_count+8);
    if(n==0)ui_navigate_to(M_NETWORK_STATUS);
    if(n==1 && !v.running)lab_preset=(lab_preset+1)%3;
    if(n==6){if(v.running)message_service_lab_stop();else if(!message_service_lab_start(lab_target,lab_preset))notice("Select target / enable mesh");}
    if(n==7){lab_refresh();s_draft_list_enter=true;}
    if(n>=8 && !v.running){memcpy(lab_target,lab_peers[n-8].mac,6);memcpy(lab_name,lab_peers[n-8].name,14);}
}
static unsigned gift_asset(void){return gift_choice<3?0:gift_choice-2;}
static unsigned gift_amount(void){static const unsigned amounts[]={10,25,50,1,1,1};return amounts[gift_choice];}
static const char *gift_asset_name(unsigned asset){static const char *const names[]={"GOLD","APPLE","SNACK","SOAP"};return ui_zh_lookup(names[asset<4?asset:0]);}
static const char *gift_stage_name(unsigned stage){static const char *const names[]={"No gift","Escrow: awaiting accept","Committing: await receipt","Cancelling: await refund","Gift delivered","Gift refunded"};return ui_zh_lookup(names[stage<=GIFT_REFUNDED?stage:0]);}
static bool gift_preview_matches(const gift_tx_t *t){return t->id==gift_preview.id && t->channel==gift_preview.channel && t->scope==gift_preview.scope && t->asset==gift_preview.asset && t->amount==gift_preview.amount && !memcmp(t->peer,gift_preview.peer,6);}
static void gift_confirm_page(pet_gifts_t *s){
    char asset[96],peer[64];snprintf(asset,sizeof(asset),"- %s x%u",gift_asset_name(gift_preview.asset),gift_preview.amount);snprintf(peer,sizeof(peer),ui_zh_lookup("- Peer [%02X%02X] ID:%lu"),gift_preview.peer[4],gift_preview.peer[5],(unsigned long)gift_preview.id);
    M_SELECT rows[]={{"- Cancel"},{asset},{peer},{"- Confirm gift change"}};int n=menu(rows,COUNT(rows));if(n==0){gift_confirm=0;s_draft_list_enter=true;return;}if(n!=3)return;
    bool ok=false;if(s->channel==gift_preview.channel && s->scope==gift_preview.scope){if(gift_confirm==1)ok=pet_gifts_offer(s,gift_preview.peer,gift_preview.asset,gift_preview.amount);else if(gift_confirm==4 && gift_preview_matches(&s->book.outgoing))ok=pet_gifts_cancel(s);else if((gift_confirm==2 || gift_confirm==3) && gift_preview_matches(&s->incoming))ok=pet_gifts_decide(s,gift_confirm==2);}
    if(!ok){notice("Gift blocked: funds/cap/storage");return;}gift_confirm=0;s_draft_list_enter=true;
}
static void gift_page(void){
    pet_gifts_t *s=gift_runtime_state();if(gift_confirm){gift_confirm_page(s);return;}
    M_SELECT *rows=extra_rows;char (*label)[128]=extra_labels;const pet_t *p=pet_service_state();const gift_tx_t *out=&s->book.outgoing;
    snprintf(label[0],128,"%s",ui_zh_lookup(s->enabled?"- Network settings":"- Enable network first"));
    snprintf(label[1],128,ui_zh_lookup("- Target: %.13s [%02X%02X]"),gift_name[0]?gift_name:"?",gift_target[4],gift_target[5]);
    snprintf(label[2],128,ui_zh_lookup("- Gift: %s x%u"),gift_asset_name(gift_asset()),gift_amount());
    snprintf(label[3],128,"%s",ui_zh_lookup("- Send gift: preview"));
    snprintf(label[4],128,"- %s #%lu %s x%u",gift_stage_name(out->stage),(unsigned long)out->id,gift_asset_name(out->asset),out->amount);
    snprintf(label[5],128,"%s",ui_zh_lookup("- Accept incoming gift"));snprintf(label[6],128,"%s",ui_zh_lookup("- Decline incoming gift"));snprintf(label[7],128,"%s",ui_zh_lookup("- Refresh gift peers"));
    if(s->error || pet_service_error())snprintf(label[8],128,"%s",ui_zh_lookup("Gift blocked: funds/cap/storage"));else snprintf(label[8],128,ui_zh_lookup("- Gold:%u apple:%u snack:%u soap:%u"),p->coins,p->items[0],p->items[1],p->items[2]);
    if(s->offer)snprintf(label[9],128,ui_zh_lookup("- From %.12s: %s x%u"),s->incoming_name,gift_asset_name(s->incoming.asset),s->incoming.amount);
    else if(out->stage>=GIFT_OFFERING && out->stage<=GIFT_CANCELLING && (out->channel!=s->channel || out->scope!=s->scope))snprintf(label[9],128,ui_zh_lookup("- Resume CH%u room:%08lX"),out->channel,(unsigned long)out->scope);
    else{const gift_tx_t *prepared=NULL;for(unsigned i=0;i<GIFT_PEERS;i++)if(s->book.receipts[i].stage==GIFT_PREPARED){prepared=&s->book.receipts[i];break;}if(prepared)snprintf(label[9],128,ui_zh_lookup("- Waiting CH%u room:%08lX"),prepared->channel,(unsigned long)prepared->scope);else snprintf(label[9],128,"%s",ui_zh_lookup("- Both open this gift page"));}
    for(unsigned i=0;i<gift_count;i++){snprintf(label[i+10],128,"- %.12s [%02X%02X]",gift_peers[i].name,gift_peers[i].mac[4],gift_peers[i].mac[5]);}
    for(unsigned i=0;i<gift_count+10;i++){rows[i].m_select=label[i];}
    int n=menu(rows,gift_count+10);if(n==0)ui_navigate_to(M_NETWORK_STATUS);if(n==2)gift_choice=(gift_choice+1)%6;
    if(n==3){gift_confirm=1;ui.select[ui.layer]=0;gift_preview=(gift_tx_t){.asset=gift_asset(),.amount=gift_amount(),.channel=s->channel,.scope=s->scope};memcpy(gift_preview.peer,gift_target,6);s_draft_list_enter=true;}
    if(n==4 && out->stage==GIFT_OFFERING){gift_confirm=4;ui.select[ui.layer]=0;gift_preview=*out;s_draft_list_enter=true;}
    if((n==5 || n==6) && s->offer){gift_confirm=n==5?2:3;ui.select[ui.layer]=0;gift_preview=s->incoming;s_draft_list_enter=true;}
    if(n==7){gift_refresh();s_draft_list_enter=true;}
    if(n>=10){memcpy(gift_target,gift_peers[n-10].mac,6);memcpy(gift_name,gift_peers[n-10].name,13);}
}
