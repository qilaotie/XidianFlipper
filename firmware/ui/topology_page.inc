/* Only local beacon observations are drawn as edges. No inferred relay path. */
static void topology_text(int x,int y,const char *s){u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);u8g2_DrawStr(&u8g2,x,y,s);}
static void topology_box(int x,int y,int w,int h){u8g2_DrawHLine(&u8g2,x,y,w);u8g2_DrawHLine(&u8g2,x,y+h-1,w);u8g2_DrawVLine(&u8g2,x,y,h);u8g2_DrawVLine(&u8g2,x+w-1,y,h);}
static const char *topology_type(unsigned type){switch(type){case MESH_TYPE_QUICK_MSG:return "TEXT";case MESH_TYPE_RECEIPT:return "ACK";case MESH_TYPE_COLLAB:return "COLLAB";case MESH_TYPE_SOCIAL:return "ROOM";case MESH_TYPE_MORSE:return "MORSE";default:return "CTRL";}}
static void topology_page(void){
    static message_network_view_t net;message_service_network_view(&net);
    unsigned peers[ROUTE_PEERS],events[ROUTE_EVENTS],np=0,ne=0;
    for(unsigned i=0;i<ROUTE_PEERS;i++)if(net.peers[i].used && net.now-net.peers[i].seen<15000)peers[np++]=i;
    for(unsigned i=0;i<ROUTE_EVENTS;i++)if(net.events[i].used && net.now-net.events[i].at<15000){unsigned j=ne++;while(j && net.now-net.events[events[j-1]].at>net.now-net.events[i].at){events[j]=events[j-1];j--;}events[j]=i;}
    if(btn.pressed){unsigned key=btn.id;btn.pressed=false;
        if(key==BTN_ID_BACK){back();return;}
        if(key==BTN_ID_LP){ui_navigate_to(M_NETWORK_DIAGNOSTICS);return;}
        if(key==BTN_ID_SP){if(!net.enabled){ui_navigate_to(M_NETWORK_STATUS);return;}s_topology_mode=(s_topology_mode+1)%4;s_topology_pick=0;}
        if(key==BTN_ID_CW || key==BTN_ID_CC){unsigned count=s_topology_mode==0?np:ne;if(count)s_topology_pick=(s_topology_pick+count+(key==BTN_ID_CW?1:count-1))%count;}
    }
    u8g2_SetDrawColor(&u8g2,1);u8g2_SetFont(&u8g2,u8g2_font_5x7_tf);char s[64];
    if(!net.enabled){text_line(11,"Mesh is off");text_line(23,"Tap opens network settings");return;}
    if(s_topology_mode==0){
        if(!np){text_line(11,"No direct beacon heard");text_line(23,"Tap: inspect message events");return;}
        s_topology_pick%=np;unsigned first=(s_topology_pick/2)*2;
        if(net.paused)snprintf(s,sizeof(s),"PAUSE N%u",np);else snprintf(s,sizeof(s),"CH%u N%u",message_service_get_channel(),np);topology_text(0,7,s);topology_box(0,9,25,13);topology_text(5,19,"ME");
        for(unsigned j=0;j<2 && first+j<np;j++){
            const route_peer_t *p=&net.peers[peers[first+j]];int y=1+j*12;
            u8g2_DrawLine(&u8g2,25,15,75,y+5);topology_box(76,y,51,10);
            snprintf(s,sizeof(s),"%u %.5s",first+j+1,p->name[0]?p->name:"PEER");topology_text(78,y+8,s);
            if(first+j==s_topology_pick)u8g2_DrawHLine(&u8g2,76,y+10,51);
        }
        snprintf(s,sizeof(s),"P%u %ddBm SP:MODE",s_topology_pick+1,net.peers[peers[s_topology_pick]].rssi);topology_text(0,31,s);return;
    }
    if(s_topology_mode==1){
        if(!ne){text_line(11,"No recent routed events");text_line(23,"Tap: counters; hold: details");return;}
        s_topology_pick%=ne;const route_event_t *e=&net.events[events[s_topology_pick]];
        if(mesh_packet_is_broadcast(e->dst))snprintf(s,sizeof(s),"%02X%02X > ALL",e->src[4],e->src[5]);else snprintf(s,sizeof(s),"%02X%02X > %02X%02X",e->src[4],e->src[5],e->dst[4],e->dst[5]);topology_text(0,8,s);
        snprintf(s,sizeof(s),"%s H%u #%08lX",topology_type(e->type),e->hops,(unsigned long)e->id);topology_text(0,19,s);
        snprintf(s,sizeof(s),"LRSSI%d AGE%lus",e->rssi,(unsigned long)((net.now-e->at)/1000));topology_text(0,30,s);return;
    }
    if(s_topology_mode==2){snprintf(s,sizeof(s),"FWD %lu",(unsigned long)net.forwarded);topology_text(0,8,s);snprintf(s,sizeof(s),"DUP %lu",(unsigned long)net.duplicates);topology_text(0,19,s);snprintf(s,sizeof(s),"DROP %lu",(unsigned long)net.dropped);topology_text(0,30,s);return;}
    ui_zh_draw_fit(&u8g2,1,11,"Edges: direct beacons only",126);ui_zh_draw_fit(&u8g2,1,23,"Relay path is unknown",126);topology_text(0,31,"SP:MODE HOLD:DIAG");
}
