#include "camera_remote.h"
#include "tool_hid_report.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    uint8_t bytes[8];
    assert(CAMERA_KEY_COUNT==4 && CAMERA_PULSE_COUNT==3);
    for(unsigned i=0;i<CAMERA_KEY_COUNT;i++){
        assert(tool_hid_encode(camera_keys[i].media,camera_keys[i].usage,bytes));
        if(camera_keys[i].media){
            assert(bytes[0]==camera_keys[i].usage && !bytes[1]);
            for(unsigned j=2;j<8;j++)assert(!bytes[j]);
        }else{
            assert(bytes[2]==camera_keys[i].usage);
            for(unsigned j=0;j<8;j++)if(j!=2)assert(!bytes[j]);
        }
    }
    assert(camera_keys[0].media && camera_keys[0].usage==0xE9);
    assert(camera_keys[1].media && camera_keys[1].usage==0xEA);
    assert(!camera_keys[2].media && camera_keys[2].usage==0x28);
    assert(!camera_keys[3].media && camera_keys[3].usage==0x2C);
    for(unsigned i=0;i<CAMERA_PULSE_COUNT;i++)assert(tool_hid_pulse_valid(camera_pulses[i]));
    assert(tool_hid_pulse_valid(45));
    assert(!tool_hid_pulse_valid(0) && !tool_hid_pulse_valid(29) && !tool_hid_pulse_valid(251));
    assert(!tool_hid_encode(true,0,bytes) && !tool_hid_encode(true,0x400,bytes));
    assert(!tool_hid_encode(false,0x66,bytes));
    puts("PASS: camera key mappings, unchanged report payloads, bounded pulse choices, original 45ms default");
}
